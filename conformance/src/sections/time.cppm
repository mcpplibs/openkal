// okc.time --- the section that examines openkal.time.
//
// The interface is one function. A section records observations through
// okc.report and returns; whether it examined anything is decided inside, by
// the same feature that decided whether the interface is provided at all.
export module okc.time;

export namespace okc::time {
void run();
}
