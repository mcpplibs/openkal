// openkal.kit.channel --- bytes carried from one context to another, inside one
// address space.
//
// WHY THIS IS NOT AN INTERFACE OF THE SPECIFICATION. Every part of it is
// composed from what openkal already has: a region from openkal.memory, and
// openkal.task's wait and wake to make a reader sleep rather than spin. A
// facility that can be written in terms of the atoms is one the specification
// declines to admit, and this module is where such a facility goes instead.
//
// It is deliberately NOT `kal_process_channel'. That interface carries a stream
// across a spawn boundary and is a kernel facility because a child process
// cannot be handed a pointer into its parent's memory. This one is within one
// address space, needs nothing from the kernel but a place to sleep, and is
// therefore the composed half of the same idea.
module;
#include <openkal/memory.h>
#include <openkal/task.h>
#include <openkal/types.h>

export module openkal.kit.channel;

import openkal.types;

namespace kal::kit::detail {

// A power of two, so that the wrap is a mask rather than a division. A division
// on every byte is what a ring buffer written without thinking costs, and this
// one is meant to be usable in a place that has no divider in hardware.
inline constexpr kal_uintptr capacity = 4096;
static_assert((capacity & (capacity - 1)) == 0, "the wrap is a mask");

struct ring {
    // ⚠️ THE INDICES ARE THE WORDS THAT ARE WAITED UPON, so they are the width
    // kal_task_wait takes and not a machine word. A wait upon a word of another
    // width is not a question the interface can be asked.
    kal_u32 head;      // written by the writer, read by the reader
    kal_u32 tail;      // written by the reader, read by the writer
    kal_u32 closed;    // set once by either end
    unsigned char bytes[capacity];
};

}  // namespace kal::kit::detail

export namespace kal::kit {

// One end of a channel. Copying an end does not duplicate it: both copies name
// the same ring, and closing either closes it. The type is a handle, as
// everything in openkal is.
struct channel_end {
    void* p;
    bool  is_reader;
};

struct channel_pair {
    channel_end reader;
    channel_end writer;
    bool        ok;
};

// Creates a channel. Both ends refer to one region; releasing is `close' on each
// end, and the region goes when the second one closes.
inline channel_pair channel_open() {
    using detail::ring;
    void* p = kal_alloc(sizeof(ring), alignof(ring));
    if (p == nullptr) return { {nullptr, true}, {nullptr, false}, false };

    auto* r = static_cast<ring*>(p);
    r->head = 0; r->tail = 0; r->closed = 0;
    return { {p, true}, {p, false}, true };
}

// How many bytes are held. Reported for a caller deciding whether to write, and
// not as a promise: another context may consume between the enquiry and the act.
inline kal_uintptr channel_pending(channel_end e) {
    using detail::ring;
    if (e.p == nullptr) return 0;
    auto* r = static_cast<ring*>(e.p);
    return static_cast<kal_uintptr>(r->head - r->tail);
}

// Writes, sleeping while the ring is full.
//
// Reports what it wrote. A short write occurs only when the far end closed, and
// is distinguished from a complete one by the count rather than by an error,
// because a caller that wrote half its buffer needs to know how much.
inline kal_uintptr channel_write(channel_end e, const void* buf, kal_uintptr len) {
    using detail::ring;
    using detail::capacity;
    if (e.p == nullptr || e.is_reader) return 0;

    auto* r = static_cast<ring*>(e.p);
    const auto* src = static_cast<const unsigned char*>(buf);
    kal_uintptr done = 0;

    while (done < len) {
        if (r->closed) break;

        const kal_u32 head = r->head;
        const kal_u32 tail = r->tail;
        const kal_uintptr used = static_cast<kal_uintptr>(head - tail);
        if (used == capacity) {
            // Full. Sleep upon the index the READER moves, so that the reader's
            // wake reaches this context. Waiting upon our own index would be a
            // wait nobody wakes.
            kal_task_wait(&r->tail, tail, 0);
            continue;
        }

        const kal_uintptr room = capacity - used;
        kal_uintptr n = len - done;
        if (n > room) n = room;
        for (kal_uintptr i = 0; i < n; ++i)
            r->bytes[(head + i) & (capacity - 1)] = src[done + i];

        r->head = head + static_cast<kal_u32>(n);
        done += n;
        kal_uintptr woken = 0;
        kal_task_wake(&r->head, 1, &woken);
    }
    return done;
}

// Reads, sleeping while the ring is empty.
//
// Zero with the channel still open cannot occur: this returns only when it has
// bytes or when the far end has closed, which is what makes zero mean end of
// input as it does for kal_stream_read.
inline kal_uintptr channel_read(channel_end e, void* buf, kal_uintptr len) {
    using detail::ring;
    using detail::capacity;
    if (e.p == nullptr || !e.is_reader || len == 0) return 0;

    auto* r = static_cast<ring*>(e.p);
    auto* dst = static_cast<unsigned char*>(buf);

    for (;;) {
        const kal_u32 head = r->head;
        const kal_u32 tail = r->tail;
        const kal_uintptr used = static_cast<kal_uintptr>(head - tail);

        if (used == 0) {
            // ⚠️ THE CLOSE IS TESTED AFTER THE INDICES AND NOT BEFORE. A writer
            // that filled the ring and closed in the same breath leaves bytes
            // behind it; a reader that saw the close first would discard them
            // and report an end of input that lost data.
            if (r->closed) return 0;
            kal_task_wait(&r->head, head, 0);
            continue;
        }

        kal_uintptr n = used < len ? used : len;
        for (kal_uintptr i = 0; i < n; ++i)
            dst[i] = r->bytes[(tail + i) & (capacity - 1)];

        r->tail = tail + static_cast<kal_u32>(n);
        kal_uintptr woken = 0;
        kal_task_wake(&r->tail, 1, &woken);
        return n;
    }
}

// Closes one end.
//
// Both indices are woken, because a context may be asleep upon either: a writer
// waiting for room and a reader waiting for bytes must both learn that no more
// is coming. Waking only the one this end moves is the deadlock this arrangement
// invites.
inline void channel_close(channel_end e) {
    using detail::ring;
    if (e.p == nullptr) return;
    auto* r = static_cast<ring*>(e.p);
    r->closed = 1;
    kal_uintptr woken = 0;
    kal_task_wake(&r->head, 64, &woken);
    kal_task_wake(&r->tail, 64, &woken);
}

// Releases the region. Called once, after both ends are closed; the pair is the
// unit of ownership and neither end owns it alone.
inline void channel_destroy(channel_pair& c) {
    using detail::ring;
    if (c.reader.p == nullptr) return;
    kal_free(c.reader.p, sizeof(ring), alignof(ring));
    c.reader.p = nullptr;
    c.writer.p = nullptr;
    c.ok = false;
}

}  // namespace kal::kit
