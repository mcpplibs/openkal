#include <unistd.h>
import openkal.stream;

extern "C" {
kal_stream kal_stdin (void) { return kal_stream{0}; }
kal_stream kal_stdout(void) { return kal_stream{1}; }
kal_stream kal_stderr(void) { return kal_stream{2}; }
kal_io_result kal_stream_write(kal_stream s, const void* b, kal_uintptr n) {
    const auto r = ::write(static_cast<int>(s.h), b, n);
    return r < 0 ? kal_io_result{ 0, kal_err_io }
                 : kal_io_result{ static_cast<kal_uintptr>(r), kal_ok };
}
kal_io_result kal_stream_read(kal_stream s, void* b, kal_uintptr n) {
    const auto r = ::read(static_cast<int>(s.h), b, n);
    return r < 0 ? kal_io_result{ 0, kal_err_io }
                 : kal_io_result{ static_cast<kal_uintptr>(r), kal_ok };
}
int kal_stream_flush(kal_stream) { return kal_ok; }
}
