// A second implementation of openkal for one target, and the only one there has
// ever been.
//
// ⚠️⚠️ WHY THIS EXISTS. Everything else in this repository asserts that ONE
// artifact, built and run in one place, behaves as the specification says. The
// property a distributed binary rests upon is different and stronger: that ONE
// BINARY, BUILT ONCE, behaves as specified against an implementation IT WAS NOT
// COMPILED AGAINST. Nothing could observe that, because every target had exactly
// one implementation and it was linked in --- so the claim was not untested, it
// was unconstructible.
//
// This is built FROM the implementation beneath it: the same objects, with four
// names renamed out of the way and answered here instead. It is therefore a
// second implementation in the only sense that matters to a consumer --- a
// different shared object, exporting the same surface, answering differently.
//
// The four are chosen because each is a path nothing else in this repository
// can reach:
//
//   the memory granularity   a different number, so a consumer that fixed it
//                            when it was built is caught
//   the enquiry's `present'  identity withheld, so the branch where a caller is
//                            TOLD it does not know is taken
//   the interfaces           `openkal.space' and `openkal.exec' declined, so
//                            the word and the absence are observed to agree
//   the version              older than the declarations a consumer holds, so
//                            its floor check is exercised
//
// ⭐ AND `KAL_EXEC_PROP_AVAILABLE' IS CLEARED HERE. Clause 6.5 objects to
// reporting availability at run time on the ground that a path no artifact takes
// is a path nothing has verified. This is the artifact that takes it.
#include <openkal.h>

// The names the implementation beneath was built with, renamed out of the way so
// that these may take them.
extern "C" {
kal_uintptr okabi_under_memory_granularity(void);
int         okabi_under_fs_info(struct kal_dir, const char*, kal_uintptr,
                                kal_uintptr, kal_u32, struct kal_node_info*);
int         okabi_under_fs_file_info(struct kal_file, kal_u32,
                                     struct kal_node_info*);
}

extern "C" {

// Sixty-four kilobytes, which is what one real system reports and this one does
// not. A consumer that rounds to what it is told stays correct; one that fixed
// four kilobytes when it was built does not.
kal_uintptr kal_memory_granularity(void) { return 65536u; }

kal_u64 kal_version(void) {
    return KAL_VERSION_MAKE(KAL_VERSION_MAJOR, KAL_VERSION_MINOR - 1u, 0u);
}

kal_u64 kal_interfaces(void) {
    return KAL_IFACE_ABORT    | KAL_IFACE_STREAM   | KAL_IFACE_MEMORY
         | KAL_IFACE_ENV      | KAL_IFACE_TIME     | KAL_IFACE_RANDOM
         | KAL_IFACE_FS       | KAL_IFACE_PROCESS  | KAL_IFACE_TASK
         | KAL_IFACE_TERMINAL | KAL_IFACE_NET      | KAL_IFACE_DATAGRAM
         | KAL_IFACE_TIMEOUT;
}

kal_uintptr kal_exec_props(void) { return 0; }

// An implementation that cannot distinguish one node from another. It leaves the
// position clear, which is what tells a caller it does not know --- as against
// answering a constant, which tells a caller that two different nodes are one.
static void withhold_identity(struct kal_node_info* out) {
    if (out == nullptr) return;
    if (out->self_size >= __builtin_offsetof(struct kal_node_info, size)) {
        out->present &= ~(kal_u32)KAL_INFO_IDENTITY;
    }
}

int kal_fs_info(struct kal_dir base, const char* name, kal_uintptr len,
                kal_uintptr flags, kal_u32 wanted, struct kal_node_info* out) {
    const int e = okabi_under_fs_info(base, name, len, flags, wanted, out);
    if (e == kal_ok) withhold_identity(out);
    return e;
}

int kal_fs_file_info(struct kal_file f, kal_u32 wanted, struct kal_node_info* out) {
    const int e = okabi_under_fs_file_info(f, wanted, out);
    if (e == kal_ok) withhold_identity(out);
    return e;
}

}  // extern "C"
