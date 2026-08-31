// ⭐⭐ THAT `openkal.macros' CAN BE IMPORTED AND ITS NAMES USED --- which is a
// different claim from the one CI already makes about it.
//
// ⚠️⚠️ THE MODULE WAS ADDED TO STOP A DEFECT CLASS AND HAD THE SAME DEFECT. A
// macro does not cross a module boundary, so every KAL_ macro is regenerated
// here under a `_M' spelling, and continuous integration regenerates the file
// and diffs it. That step asserts the file is UP TO DATE. It does not assert
// that anybody can use it, and until this translation unit was written nothing
// in this ecosystem had ever imported the module --- so "generated correctly"
// was checked on every commit and "reaches a consumer" was checked never.
//
// It was found the first time something tried: a conformance check in
// openkal-macos named `KAL_PROCESS_PROP_STOP_REQUESTED_M' unqualified, because
// the module's own comment says the C spelling "always exists here too", and
// the name is in fact `kal::macros::KAL_PROCESS_PROP_STOP_REQUESTED_M'. The
// qualification is right --- it is the house style every other module follows
// --- and what was missing was a consumer to state it.
//
// ⇒ This file is that consumer. It runs nothing: a `static_assert' upon a value
// fails the build both when the NAME does not resolve and when the VALUE is
// wrong, which are the two ways the generated file could betray a caller.

import openkal.macros;

namespace {

// The values are those the headers define. Written out rather than compared
// against the macro, because in a module translation unit the macro is exactly
// what is not available --- which is the whole reason this module exists.
static_assert(kal::macros::KAL_PROCESS_PROP_TERMINATE_M      == 1u);
static_assert(kal::macros::KAL_PROCESS_PROP_STREAM_PASSING_M == 2u);
static_assert(kal::macros::KAL_PROCESS_PROP_EXIT_STATUS_M    == 4u);
static_assert(kal::macros::KAL_PROCESS_PROP_CHANNEL_M        == 8u);
static_assert(kal::macros::KAL_PROCESS_PROP_GRANT_DIR_M      == 16u);
static_assert(kal::macros::KAL_PROCESS_PROP_BOUND_LIFETIME_M == 32u);
static_assert(kal::macros::KAL_PROCESS_PROP_JOB_M            == 64u);
static_assert(kal::macros::KAL_PROCESS_PROP_STOP_REQUESTED_M == 128u);

// One from another interface, so that a generator which emitted only the last
// header it read would be caught.
static_assert(kal::macros::KAL_SPAWN_BOUND_LIFETIME_M == 1u);

}  // namespace
