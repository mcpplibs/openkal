// The version the declarations state is the version the package is.
//
// `kal_version' is `return KAL_VERSION;' in every implementation, so the three
// constants of version.h are what a consumer bound at load can ask, and clause
// 6.2's comparison is made against them. They drifted through 0.12.0 and 0.13.0,
// when the comparison was between two equal numbers on every implementation.
// The manifest is the authority: it is what the index publishes and what a
// consumer resolves.
//
// The constants are read by the compiler, not by a pattern over the header's
// text, so a renamed constant is a build failure rather than a comparison that
// read nothing.
#include <openkal/version.h>

import std;
import mcpplibs.mtest;

using namespace mcpplibs::mtest;

namespace {

// The `version = "..."` of the [package] table.
std::optional<std::string> packaged(std::string_view manifest) {
    static const std::regex line{R"re(^version\s*=\s*"([^"]*)")re", std::regex::multiline};
    std::match_results<std::string_view::const_iterator> m;
    if (!std::regex_search(manifest.begin(), manifest.end(), m, line)) return std::nullopt;
    return m[1].str();
}

std::string read(const std::filesystem::path& file) {
    std::ifstream in{file, std::ios::binary};
    return {std::istreambuf_iterator<char>{in}, {}};
}

UTest version{"version", [] {
    const auto declared = std::format("{}.{}.{}", KAL_VERSION_MAJOR, KAL_VERSION_MINOR,
                                      KAL_VERSION_PATCH);

    "the declarations state the package's version"_test = [&] {
        const auto text = read("mcpp.toml");
        expect(fatal(!text.empty())) << "mcpp.toml is read from the package root";
        const auto v = packaged(text);
        expect(fatal(v.has_value())) << "the manifest declares a version";
        expect(eq(declared, *v)) << "every implementation answers kal_version with the "
                                    "first; edit include/openkal/version.h to the second";
    };

    "a manifest without a version is not read as one"_test = [] {
        expect(!packaged("[package]\nname = \"openkal\"\n").has_value());
        expect(eq(*packaged("[package]\nversion = \"9.8.7\"\n"), std::string{"9.8.7"}));
    };
}};

}  // namespace
