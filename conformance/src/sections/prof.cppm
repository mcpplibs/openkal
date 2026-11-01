// okc.prof --- the section that examines openkal.prof (version 0.20).
//
// What a measurement does with a counter is outside the program, so what is
// observed here is the program's side: the answer to whether it is measured
// does not change, a counter and a mark are accepted whether or not it is, and
// they cost nothing visible when it is not.
export module okc.prof;

export namespace okc::prof {
void run();
}
