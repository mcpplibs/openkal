import openkal.decl.stream;
import openkal.decl.types;

extern "C" {
kal_stream kal_stdin (void) { return kal_stream{0}; }
kal_stream kal_stdout(void) { return kal_stream{1}; }
kal_stream kal_stderr(void) { return kal_stream{2}; }
kal_io_result kal_stream_write(kal_stream, const void*, kal_uintptr len) { return { len, kal_ok }; }
kal_io_result kal_stream_read (kal_stream, void*, kal_uintptr)           { return { 0, kal_ok }; }
int           kal_stream_flush(kal_stream)                                { return kal_ok; }
}
