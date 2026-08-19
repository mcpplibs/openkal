// This source is compiled unchanged against both implementations. It names
// neither, and it contains no conditional compilation.
import openkal.stream;

int main() {
    const char line[] = "the application produced this line\n";
    kal::write(kal::out(), line, sizeof(line) - 1);
    return 0;
}
