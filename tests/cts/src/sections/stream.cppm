// okc.stream --- the section that examines openkal.stream.
//
// The interface is one function. A section records observations through
// okc.report and returns; whether it examined anything is decided inside, by
// the same feature that decided whether the interface is provided at all.
export module okc.stream;

export namespace okc::stream {
void run();
}
