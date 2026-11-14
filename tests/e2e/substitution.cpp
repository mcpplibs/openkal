// THE PROPERTY THE SPECIFICATION EXISTS FOR: one source, built against two
// implementations, behaves as each of them does, and the source does not change
// (examples/substitution). And the surface checker of clause 9 (tools/
// check-surface.sh) fails when it should and answers what an artefact provides.
//
// Each case works on a copy of the example in its temporary directory, whose
// manifests name this working tree by its absolute path, so the checkout is
// never written.
import std;
import mcpplibs.mtest;
import mcpplibs.mtest.e2e;

using namespace mcpplibs::mtest;
namespace fs = std::filesystem;

namespace {

const capability mcpp{"mcpp", {.program = {"mcpp"}}};
const capability cc{"cc", {.command = {"cc", "--version"}}};

std::string read(const fs::path& file) {
    std::ifstream in{file, std::ios::binary};
    return {std::istreambuf_iterator<char>{in}, {}};
}

void write(const fs::path& file, std::string_view text) {
    std::ofstream{file, std::ios::binary} << text;
}

// The line of `file' that begins with `key', and the words after it, replaced by `to'.
void replace(const fs::path& file, std::string_view key, std::string_view to) {
    auto text = read(file);
    const std::regex line{std::format("(^|\n){}\\s*=[^\n]*", key)};
    expect(fatal(std::regex_search(text, line))) << file.generic_string() << " states " << key;
    write(file, std::regex_replace(text, line, std::format("$1{}", to)));
}

// examples/substitution without what a build left there, naming this tree.
fs::path example() {
    const auto to = temp_dir() / "substitution";
    for (auto it = fs::recursive_directory_iterator{"examples/substitution"};
         it != fs::recursive_directory_iterator{}; ++it) {
        const auto leaf = it->path().filename();
        if (leaf == "target" || leaf == ".mcpp" || leaf == "mcpp.lock") {
            it.disable_recursion_pending();
            continue;
        }
        const auto at = to / it->path().lexically_relative("examples/substitution");
        if (it->is_directory()) fs::create_directories(at);
        else fs::copy_file(it->path(), at);
    }
    const auto spec = std::format("openkal = {{ path = \"{}\" }}", fs::absolute(".").generic_string());
    for (auto p : {"app", "impl-fd", "impl-discard"})
        replace(to / p / "mcpp.toml", "openkal", spec);
    return to;
}

e2e::outcome mcpp_in(const fs::path& dir, std::vector<std::string> args) {
    args.insert(args.begin(), "mcpp");
    return e2e::run({.argv = std::move(args), .cwd = dir, .timeout = std::chrono::minutes{10}});
}

// Every object file a build of `dir` produced.
std::vector<std::string> objects(const fs::path& dir) {
    std::vector<std::string> out;
    for (auto& e : fs::recursive_directory_iterator{dir / "target"})
        if (e.is_regular_file() && e.path().extension() == ".o") out.push_back(e.path().string());
    return out;
}

e2e::outcome surface(std::vector<std::string> args) {
    args.insert(args.begin(), {"bash", fs::absolute("tools/check-surface.sh").string()});
    return e2e::run({.argv = std::move(args), .timeout = std::chrono::minutes{2}});
}

std::set<std::string> lines(std::string_view text) {
    std::set<std::string> out;
    for (auto part : std::views::split(text, '\n')) out.emplace(std::string_view{part});
    return out;
}

E2E substitution{"substitution", mcpp, except{platform::windows}, timeout{std::chrono::minutes{20}}, [] {
    const auto produced = std::string_view{"the application produced this line"};

    "one source over two implementations"_test = [&] {
        const auto root = example();
        const auto app = root / "app";
        const auto before = read(app / "src" / "main.cpp");

        const auto with_fd = mcpp_in(app, {"run"});
        expect(fatal(e2e::succeeded(with_fd)));
        expect(e2e::contains(with_fd.out + with_fd.err, produced));

        replace(app / "mcpp.toml", "openkal-fd", "openkal-discard = { path = \"../impl-discard\" }");
        fs::remove_all(app / "target");
        const auto with_discard = mcpp_in(app, {"run"});
        expect(with_discard.started && !with_discard.timed_out);
        expect(!(with_discard.out + with_discard.err).contains(produced))
            << "the discarding implementation produced output";

        expect(eq(read(app / "src" / "main.cpp"), before)) << "the source changed between builds";
    };

    // The checker is only useful if it fails when it should: an unspecified
    // name is introduced and must be rejected.
    "the surface checker rejects an addition"_test = [&] {
        const auto impl = example() / "impl-fd";
        const auto list = fs::absolute("SURFACE.txt").string();
        expect(fatal(e2e::succeeded(mcpp_in(impl, {"build"}))));
        auto args = objects(impl);
        args.insert(args.begin(), list);
        expect(fatal(e2e::succeeded(surface(args))));

        write(impl / "src" / "extra.cpp", "extern \"C\" void kal_vendor_extension(void) {}\n");
        expect(fatal(e2e::succeeded(mcpp_in(impl, {"build"}))));
        args = objects(impl);
        args.insert(args.begin(), list);
        const auto r = surface(args);
        expect(r.started && !r.timed_out && r.status != 0) << "the checker accepted an unspecified name";
    };

    // `provides-interfaces' in a package manifest is a declaration, checked
    // rather than trusted: the walk that decides whether each group is exported
    // whole answers which interfaces an artefact provides. The subject is an
    // object built from SURFACE.txt whose answer is known in advance: three
    // groups whole, and two names of a fourth.
    "the interface list is the artefact's own answer"_test[cc] = [&] {
        std::map<std::string, std::vector<std::string>> groups;
        std::string current;
        for (auto part : std::views::split(read("SURFACE.txt"), '\n')) {
            std::string line{std::string_view{part}};
            if (line.starts_with("# openkal.")) {
                current = line.substr(2, line.find(' ', 2) - 2);
                groups[current];
            } else if (!line.empty() && !line.starts_with('#') && !current.empty()) {
                line.erase(0, line.find_first_not_of(" \t"));
                line.erase(line.find_last_not_of(" \t\r") + 1);
                if (!line.empty()) groups[current].push_back(line);
            }
        }
        std::string fake;
        auto define = [&](const std::string& n) {
            fake += std::format("int {0}(void);\nint {0}(void) {{ return 0; }}\n", n);
        };
        for (auto g : {"openkal.version", "openkal.abort", "openkal.stream", "openkal.memory"})
            for (auto& n : groups[g]) define(n);
        for (auto& n : groups["openkal.fs"] | std::views::take(2)) define(n);

        const auto dir = e2e::tree({{"fake.c", fake}});
        expect(fatal(e2e::succeeded(
            e2e::run({.argv = {"cc", "-c", "fake.c", "-o", "fake.o"}, .cwd = dir}))));
        const auto object = (dir / "fake.o").string();
        const auto list = fs::absolute("SURFACE.txt").string();

        const auto r = surface({"--interfaces", list, object});
        expect(fatal(e2e::succeeded(r)));
        const auto provided = lines(r.out);
        for (auto want : {"openkal.abort", "openkal.stream", "openkal.memory"})
            expect(provided.contains(want)) << want << " is exported whole and was not listed";
        // openkal.version is exported by every implementation and is not an interface.
        expect(!provided.contains("openkal.version")) << "openkal.version is not an interface";
        // Half an interface is not a smaller claim.
        expect(!provided.contains("openkal.fs")) << "a group exported in part was reported as provided";

        // The --toml spelling is the same answer in the shape a manifest carries.
        const auto t = surface({"--toml", list, object});
        expect(fatal(e2e::succeeded(t)));
        const auto toml = lines(t.out);
        expect(toml.contains("provides-interfaces = ["));
        expect(toml.contains("    \"openkal.stream\","));
    };
}};

}  // namespace
