// openkal.decl.types --- definitions shared by every openkal interface.
//
// The declarations in this module are normative. An implementation package
// re-exports them unchanged; it may not redefine them. That prohibition is
// enforced by the language rather than by convention: a module that redeclares
// an imported type is rejected by the compiler.
export module openkal.decl.types;

// The width of a machine word, obtained from the compiler rather than from a
// header. A freestanding target may have no <cstdint>, and openkal is required
// to be usable on such a target.
export using kal_uintptr = __UINTPTR_TYPE__;

// The complete set of error conditions openkal defines.
//
// The set is closed. An implementation maps its platform's error values onto
// these; it does not extend the set. Mapping is not emulation: a table lookup
// preserves the property that every backend implements the interface naturally,
// whereas reproducing a foreign error namespace does not.
//
// Detail beyond these values is deliberately unavailable. A per-thread channel
// carrying the platform's own error value was considered and rejected, because
// such a channel is global mutable state with the same defects as errno, and
// because control flow that depends on it is not portable by construction.
export enum kal_error : int {
    kal_ok             = 0,
    kal_err_invalid    = 1,   // the handle or an argument is not valid
    kal_err_again      = 2,   // the operation would block and blocking was not requested
    kal_err_io         = 3,   // the device or medium reported a failure
    kal_err_no_memory  = 4,
    kal_err_no_space   = 5,
    kal_err_permission = 6,
    kal_err_not_supported = 7,
    kal_err_closed     = 8,   // the peer closed the connection
};

// The result of an operation that transfers a count.
//
// The layout of this structure is frozen. openkal's evolution rule permits new
// declarations and forbids changes to existing ones; a structure layout is not
// protected by that rule unless the layout itself is declared immutable, which
// it is here. Two machine words are returned in registers on the architectures
// openkal targets, and a wider result would be returned through a hidden
// pointer instead.
export struct kal_io_result {
    kal_uintptr n;
    int         e;
};
