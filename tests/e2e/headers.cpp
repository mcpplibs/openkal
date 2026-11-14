// The headers, examined by the tools of this repository (tools/), as a
// consumer's compiler reads them. Each case states why it exists; none changes
// the checkout, so they run beside each other.
import std;
import mcpplibs.mtest;
import mcpplibs.mtest.e2e;

using namespace mcpplibs::mtest;
namespace fs = std::filesystem;

namespace {

const capability cc{"cc", {.command = {"cc", "--version"}}};
const capability clang{"clang", {.command = {"clang", "--version"}}};

std::string read(const fs::path& file) {
    std::ifstream in{file, std::ios::binary};
    return {std::istreambuf_iterator<char>{in}, {}};
}

e2e::outcome tool(std::vector<std::string> argv,
                  std::vector<std::pair<std::string, std::optional<std::string>>> env = {}) {
    argv.insert(argv.begin(), "bash");
    return e2e::run({.argv = std::move(argv), .env = std::move(env),
                     .timeout = std::chrono::minutes{5}});
}

// The tools are POSIX shell scripts.
E2E headers{"headers", except{platform::windows}, [] {
    // EVERY MACRO HAS A MODULE SPELLING, and it is checked rather than
    // remembered: `KAL_LOCK_*' in 0.10 and `KAL_SPAWN_*' in 0.11 were each added
    // to a header, used from a module consumer, and failed to compile there,
    // because a macro does not cross a module boundary. src/macros.cppm is
    // generated, and generating it again must give the file committed.
    "every macro has its module spelling"_test = [] {
        const auto out = temp_dir() / "macros.cppm";
        expect(fatal(e2e::succeeded(tool({"tools/gen-macros.sh", out.string()}))));
        expect(eq(read(out), read("src/macros.cppm")))
            << "src/macros.cppm is out of date: run tools/gen-macros.sh and commit it";
    };

    // The C form with the environment's own headers excluded, because the
    // consumer it exists for (a C library ported onto openkal) is compiled that
    // way; and the translation unit the suite carries names the same entities.
    "the C form compiles alone under cc"_test[cc] = [] {
        expect(e2e::succeeded(tool({"tools/check-declarations.sh"}, {{"CC", "cc"}})));
    };
    "the C form compiles alone under clang"_test[clang] = [] {
        expect(e2e::succeeded(tool({"tools/check-declarations.sh"}, {{"CC", "clang"}})));
    };

    // NO DECLARATION NAMES A BACKEND TYPE: `sizeof(long)' is 8 on x86_64 Linux
    // and 4 on x86_64 Windows, so a `long' in an interface is one signature with
    // two meanings.
    "no declaration names a backend type"_test[clang] = [] {
        expect(e2e::succeeded(tool({"tools/check-types.sh", "clang", "include"})));
    };

    // And the type check fails when it should: a check that succeeds by finding
    // nothing succeeds identically when it reads nothing. A copy of the headers
    // is given a declaration in `unsigned long'.
    "a backend type in a declaration is reported"_test[clang] = [] {
        const auto copy = temp_dir() / "include";
        fs::copy("include", copy, fs::copy_options::recursive);
        std::ofstream{copy / "openkal" / "env.h", std::ios::app}
            << "\nint kal_env_probe_leak(unsigned long n);\n";
        const auto r = tool({"tools/check-types.sh", "clang", copy.string()});
        expect(r.started && !r.timed_out && r.status != 0)
            << "the type check accepted 'unsigned long' in an interface";
    };
}};

}  // namespace
