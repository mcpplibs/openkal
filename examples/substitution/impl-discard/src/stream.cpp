import openkal.stream;

extern "C" {
kal_stream kal_stdin (void) { return kal_stream{0}; }
kal_stream kal_stdout(void) { return kal_stream{1}; }
kal_stream kal_stderr(void) { return kal_stream{2}; }
// The count, or the negated condition when no byte moved. One signed word.
kal_intptr kal_stream_write(kal_stream, const void*, kal_uintptr len) { return (kal_intptr)len; }
kal_intptr kal_stream_read (kal_stream, void*, kal_uintptr)           { return 0; }
int        kal_stream_flush(kal_stream)                               { return kal_ok; }
}
