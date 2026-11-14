// THE C ABI HOLDS AT THIRTY-TWO BITS AS WELL AS SIXTY-FOUR.
//
// The ABI this repository freezes is instantiated at two pointer widths, and
// every hosted target it serves is 64-bit. `kal_node_info's assertion once read
// `offsetof(modified_ns) == sizeof(kal_uintptr)', true at 64 bits and false at
// 32, and it surfaced first in a consumer's rv32 build. The probe
// (tests/e2e/data/abi/main.cpp) is compiled for a target of each width; the
// claim is about layout, so it is compiled and not run. mcpp resolves the
// toolchain of each target itself, which holds on every row.
import std;
import mcpplibs.mtest;
import mcpplibs.mtest.e2e;

using namespace mcpplibs::mtest;
namespace fs = std::filesystem;

namespace {

const capability mcpp{"mcpp", {.program = {"mcpp"}}};

std::string read(const fs::path& file) {
    std::ifstream in{file, std::ios::binary};
    return {std::istreambuf_iterator<char>{in}, {}};
}

E2E abi{"abi", mcpp, timeout{std::chrono::minutes{15}}, [] {
    const auto include = fs::absolute("include").generic_string();
    const auto probe = read("tests/e2e/data/abi/main.cpp");
    "the frozen layout holds"_test = values<std::string>{"riscv64-none-elf", "riscv32-none-elf"} |
        [&](const std::string& target) {
            const auto manifest = std::format(
                "[package]\nname = \"abi\"\nversion = \"0.1.0\"\n\n[build]\ninclude_dirs = [\"{}\"]\n",
                include);
            const auto root = e2e::tree({{"mcpp.toml", manifest}, {"src/main.cpp", probe}});
            expect(e2e::succeeded(e2e::run({.argv = {"mcpp", "build", "--target", target},
                                            .cwd = root,
                                            .timeout = std::chrono::minutes{10}})));
        };
}};

}  // namespace
