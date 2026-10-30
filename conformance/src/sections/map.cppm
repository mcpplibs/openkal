// okc.map --- the section that examines openkal.map (version 0.18).
//
// A region is observed by its bytes: a pointer that came back and an operation
// that reported success are what an implementation returning plausible values
// would also produce. So each observation reads or writes the region and then
// reads the file through `openkal.fs', or a second region of it.
export module okc.map;

export namespace okc::map {
void run();
}
