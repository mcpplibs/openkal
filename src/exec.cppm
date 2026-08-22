// openkal.exec --- memory a program may execute.
//
// Optional, and the reason it is an interface of its own rather than a position
// in a capability word is clause 6.2: an operation an implementation may lack
// is expressed by its absence, so that a program using it without a supporting
// implementation is told by the linker. A position in a word would express the
// same fact later and less usefully, and an operation that was present and
// always refused would be the defect clause 6.1 names.
//
// Whether an implementation provides this interface can depend on how the
// artifact is produced rather than on the implementation alone --- one system
// grants a program such memory only when the program carries a signed
// declaration that it will ask. That is settled at dependency resolution, which
// clause 6.2's table already names as the earliest time such a question can be
// answered, and it is expressed as a feature of the implementation package
// rather than as anything visible here. Clause 6.6 records the arrangement.
module;
#include <openkal/exec.h>

export module openkal.exec;
export import openkal.types;

export using ::kal_exec_alloc;
export using ::kal_exec_publish;
export using ::kal_exec_free;
export using ::kal_exec_props;

export namespace kal {

inline void* exec_alloc(kal_uintptr size) { return kal_exec_alloc(size); }
inline int   exec_publish(void* p, kal_uintptr size) { return kal_exec_publish(p, size); }
inline void  exec_free(void* p, kal_uintptr size) { kal_exec_free(p, size); }

}
