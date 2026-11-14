// okc.exec --- the section that examines openkal.exec.
//
// The interface is three operations, and only one observation settles whether
// an implementation has them: that instructions written into a reserved region
// and then published are actually executed. Everything else this section could
// check --- that a pointer came back, that publishing reported success --- an
// implementation returning plausible values would satisfy without providing the
// facility at all.
export module okc.exec;

export namespace okc::exec {
void run();
}
