// okc.task --- the section that examines openkal.task.
//
// The interface is one function. A section records observations through
// okc.report and returns; whether it examined anything is decided inside, by
// the same feature that decided whether the interface is provided at all.
export module okc.task;

export namespace okc::task {
void run();
}
