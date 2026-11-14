// okc.atomic --- the four operations the suite needs, under three compilers.
//
// The suite builds a mutex out of openkal's suspension primitive, and a mutex
// needs an atomic exchange and an atomic compare-and-exchange. Two of the three
// compilers this package is built with publish those as builtins of the same
// name; the third publishes them under different names, in a different form,
// and only for a signed thirty-two-bit word.
//
// They are stated here rather than at each use, and they are stated rather than
// imported: the suite depends on openkal and the language and does not import
// std, because it must run against an implementation in a program that carries
// no other runtime. `<atomic>` is a header of the standard library even where a
// freestanding implementation is required to provide it, and a suite reporting
// on an implementation while resting on facilities that implementation may be
// the only supplier of would be reporting on itself.
//
// The ordering is release on a store and acquire on a load, which is what the
// two builtins are asked for. The third compiler's operations are full barriers
// on the two architectures this package is built for, which is stronger and
// therefore also correct; there is no weaker spelling of them, and a suite is
// not the place to want one.
export module okc.atomic;

import openkal.types;

#if defined(_MSC_VER) && !defined(__clang__)
extern "C" {
long _InterlockedExchange(long volatile*, long);
long _InterlockedCompareExchange(long volatile*, long, long);
long _InterlockedOr(long volatile*, long);
}
#pragma intrinsic(_InterlockedExchange, _InterlockedCompareExchange, _InterlockedOr)
#endif

export namespace okc {

// A word, loaded so that everything the writer did before storing it is visible
// to a reader that sees it.
inline kal_u32 load_acquire(const volatile kal_u32* p) {
#if defined(_MSC_VER) && !defined(__clang__)
    // An `or' of zero reads the word and writes back what was there, which is
    // the shortest read this compiler offers that is atomic and ordered.
    return static_cast<kal_u32>(
        _InterlockedOr(reinterpret_cast<long volatile*>(const_cast<volatile kal_u32*>(p)), 0));
#else
    return __atomic_load_n(p, __ATOMIC_ACQUIRE);
#endif
}

inline int load_acquire(const volatile int* p) {
#if defined(_MSC_VER) && !defined(__clang__)
    return static_cast<int>(
        _InterlockedOr(reinterpret_cast<long volatile*>(const_cast<volatile int*>(p)), 0));
#else
    return __atomic_load_n(p, __ATOMIC_ACQUIRE);
#endif
}

// A word, stored so that everything done before the store is visible to a
// reader that sees it.
inline void store_release(volatile kal_u32* p, kal_u32 v) {
#if defined(_MSC_VER) && !defined(__clang__)
    _InterlockedExchange(reinterpret_cast<long volatile*>(p), static_cast<long>(v));
#else
    __atomic_store_n(p, v, __ATOMIC_RELEASE);
#endif
}

inline void store_release(volatile int* p, int v) {
#if defined(_MSC_VER) && !defined(__clang__)
    _InterlockedExchange(reinterpret_cast<long volatile*>(p), static_cast<long>(v));
#else
    __atomic_store_n(p, v, __ATOMIC_RELEASE);
#endif
}

// The old value, replaced by the new one in one indivisible step.
inline kal_u32 exchange(volatile kal_u32* p, kal_u32 v) {
#if defined(_MSC_VER) && !defined(__clang__)
    return static_cast<kal_u32>(
        _InterlockedExchange(reinterpret_cast<long volatile*>(p), static_cast<long>(v)));
#else
    return __atomic_exchange_n(p, v, __ATOMIC_ACQ_REL);
#endif
}

// Replaces the word with `desired' if it holds `expected'. Reports whether it
// did, and leaves what it actually held in `expected' either way --- which is
// the form a mutex's acquisition loop wants and the form both compilers offer.
inline bool compare_exchange(volatile kal_u32* p, kal_u32& expected, kal_u32 desired) {
#if defined(_MSC_VER) && !defined(__clang__)
    const long was = _InterlockedCompareExchange(reinterpret_cast<long volatile*>(p),
                                                 static_cast<long>(desired),
                                                 static_cast<long>(expected));
    const kal_u32 seen = static_cast<kal_u32>(was);
    if (seen == expected) return true;
    expected = seen;
    return false;
#else
    return __atomic_compare_exchange_n(p, &expected, desired, false,
                                       __ATOMIC_ACQUIRE, __ATOMIC_RELAXED);
#endif
}

}  // namespace okc
