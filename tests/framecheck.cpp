#include "framecheck.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace altair;

namespace {

// `expected` as the test wrote it -> the exact bytes frameText() would produce.
std::string normalize(std::string s) {
    if (!s.empty() && s[0] == '\n') s.erase(0, 1);
    if (!s.empty() && s.back() != '\n') s.push_back('\n');
    return s;
}

std::vector<std::string> rows(const std::string& grid) {
    std::vector<std::string> out;
    std::istringstream       in(grid);
    std::string              line;
    while (std::getline(in, line)) out.push_back(line);
    return out;
}

// A filename-safe stem from the check's description.
std::string stem(const char* what) {
    std::string s;
    for (const char* p = what; *p && s.size() < 48; ++p) {
        char c = *p;
        bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
        s.push_back(ok ? c : '_');
    }
    return s.empty() ? std::string("frame") : s;
}

// The comparison and the diagnostics, shared by the inline and golden forms.
bool compare(NullDisplay& d, Display::Owner owner, const std::string& expected,
             const TextGridOpts& o, const char* what) {
    const Surface* s = d.surface(owner);
    if (!s) {
        std::printf("        frame: the board has drawn nothing -- no surface for this owner\n");
        return false;
    }
    const std::string actual = frameText(*s, o);
    if (actual == expected) return true;

    std::vector<std::string> e = rows(expected), a = rows(actual);
    std::printf("        frame: %dx%d surface, expected %zu rows of text, got %zu\n",
                s->width(), s->height(), e.size(), a.size());
    size_t n = e.size() < a.size() ? e.size() : a.size();
    for (size_t r = 0; r < n; ++r) {
        if (e[r] == a[r]) continue;
        size_t col = 0;
        while (col < e[r].size() && col < a[r].size() && e[r][col] == a[r][col]) ++col;
        std::printf("        row %zu differs at column %zu\n", r, col);
        std::printf("          expected  %s\n", e[r].c_str());
        std::printf("          actual    %s\n", a[r].c_str());
        std::printf("                    %*s^\n", (int)col, "");
        break;
    }

    // The picture itself, for a person.
    std::string path =
        (std::filesystem::temp_directory_path() / ("altair_frame_" + stem(what) + ".ppm"))
            .string();
    std::string err;
    if (writePpm(path, *s, d.palette(owner), err))
        std::printf("        actual frame written to %s\n", path.c_str());
    else
        std::printf("        (%s)\n", err.c_str());
    return false;
}

std::string goldenPath(const char* name) {
    return (std::filesystem::path(ALTAIR_SOURCE_DIR) / "tests" / "golden" /
            (std::string(name) + ".txt"))
        .string();
}

} // namespace

bool checkFrame(NullDisplay& d, Display::Owner owner, const char* expected,
                const TextGridOpts& o, const char* what) {
    return compare(d, owner, normalize(expected), o, what);
}

bool checkFrameGolden(NullDisplay& d, Display::Owner owner, const char* name,
                      const TextGridOpts& o, const char* what) {
    const std::string path = goldenPath(name);

    if (const char* w = std::getenv("ALTAIR_TEST_WRITE_GOLDEN"); w && *w && *w != '0') {
        const Surface* s = d.surface(owner);
        if (!s) {
            std::printf("        golden: nothing drawn, %s not written\n", path.c_str());
            return false;
        }
        std::filesystem::create_directories(std::filesystem::path(path).parent_path());
        std::ofstream out(path, std::ios::binary);
        out << frameText(*s, o);
        std::printf("        golden written: %s\n", path.c_str());
        return (bool)out;
    }

    std::ifstream in(path, std::ios::binary);
    if (!in) {
        std::printf("        golden missing: %s (run with ALTAIR_TEST_WRITE_GOLDEN=1 to create)\n",
                    path.c_str());
        return false;
    }
    std::stringstream buf;
    buf << in.rdbuf();
    return compare(d, owner, normalize(buf.str()), o, what);
}
