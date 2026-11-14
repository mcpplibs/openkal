// The frozen layout, compiled for a target of each pointer width (tests/e2e/abi.cpp).
#include <openkal/types.h>
#include <openkal/fs.h>
// `self_size' is first, and that is the whole of the growth rule: an
// implementation built at a later version reads it before anything else, to
// learn how much of the structure the caller has.
static_assert(__builtin_offsetof(kal_node_info, self_size) == 0);
static_assert(__builtin_offsetof(kal_node_info, present)   == 4);
// The fields of 0.9.0, at the offsets they had. Growth is past the end only.
static_assert(__builtin_offsetof(kal_node_info, size)        == 8);
static_assert(__builtin_offsetof(kal_node_info, modified_ns) == 16);
static_assert(__builtin_offsetof(kal_node_info, identity)    == 24);
static_assert(__builtin_offsetof(kal_node_info, kind)        == 40);
static_assert(__builtin_offsetof(kal_node_info, writable)    == 44);
// Appended in 0.13.
static_assert(__builtin_offsetof(kal_node_info, executable)  == 48);
static_assert(__builtin_offsetof(kal_node_info, reserved)    == 52);
// No field is pointer-sized, so the size is one at both widths.
static_assert(sizeof(kal_node_info) == 56);
static_assert(sizeof(kal_dir)  == sizeof(kal_uintptr));
static_assert(sizeof(kal_file) == sizeof(kal_uintptr));
// A transfer is one signed word of a pointer's width.
static_assert(sizeof(kal_intptr) == sizeof(kal_uintptr));
static_assert((kal_intptr)-1 < 0);
extern "C" void _start() { for (;;) {} }
