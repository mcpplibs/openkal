// okc.region --- the section that examines openkal.region (version 0.20).
//
// A range is observed by its bytes: that a part reads as zero when it is first
// backed and after its backing was returned, that a part written keeps what was
// written, and that a discarded part may be written at once. A pointer that came
// back and an operation that reported success are what an implementation
// returning plausible values would also produce.
export module okc.region;

export namespace okc::region {
void run();
}
