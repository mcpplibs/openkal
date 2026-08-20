// okc.abort --- the section that examines openkal.abort.
//
// The namespace is `termination' rather than `abort': a namespace of that name
// would shadow nothing the suite uses today and would shadow something the day
// a section reached for it, and a name that is safe only until someone writes
// the obvious line is not safe.
export module okc.abort;

export namespace okc::termination {
void run();
}
