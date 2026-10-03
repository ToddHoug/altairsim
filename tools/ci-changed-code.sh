#!/usr/bin/env bash
#
# What kind of change is this, between <base> and HEAD? Prints one word on stdout, and the
# path that decided it goes to stderr so it lands in the run log:
#
#   docs   documentation only                         -- the Linux leg is enough
#   code   code, but none of it core                  -- the Linux leg on a PR
#   core   something every platform can see differently -- all three legs
#
#   usage: bash tools/ci-changed-code.sh <base-commit>
#
# ci.yml's `scope` job decides which base to ask about and what each answer builds; this script
# only sorts paths. It is a file rather than inline YAML so the exact rule CI applies can be run
# against any commit locally. The highest level of any changed path is the answer.
#
# DOCUMENTATION = anything under docs/, reference/ or .claude/, a top-level *.md, a README.md at
# any depth, or LICENSE.
#
# .claude/ IS DOCUMENTATION even though it is prose in a nested directory. The skills there are
# instructions for an agent; nothing in them is compiled, and no test's result depends on the
# platform it is read on. One of them (.claude/skills/altairsim) does ship inside the release
# package, and build-package.sh checks it -- but that check is platform-independent like
# docs-manual, so the Linux leg covers it.
#
# A README.md IS DOCUMENTATION WHEREVER IT IS. It is prose for someone standing in that
# directory: no glob embeds it (machines/ takes *.toml, roms/ takes folders), no test reads one,
# and the Linux leg still runs the whole suite. ONLY THAT FILE NAME moves -- any other nested
# .md (a PROVENANCE.md, a note under tests/media/) is code. PR #610 added a README to nine
# directories and built three operating systems to prove nine Markdown files compile to nothing.
#
# CODE that is not core = the boards and chips, the CLI and MCP front ends, the tests, the data
# the tests read (machines, roms, tapes, disks, cpm, examples), any other nested .md, and the
# tools other than the CI scripts. That is most PRs. They are portable C++ above the platform
# layer, so a PR builds them on Linux only. The three-leg run on master after the merge is what
# catches an MSVC- or Clang-only warning in them, minutes later rather than at release.
#
# CORE = everything else: src/core, cpu, isa, platform, host (the SDL display, which only the
# macOS leg compiles), config, util, src/main.cpp, the build (CMakeLists.txt, cmake/), the
# workflows and the CI scripts. AN UNRECOGNIZED PATH IS CORE, so a new directory is never built
# on fewer platforms than it needs.
#
# ADDING A BOARD EDITS CMakeLists.txt, AND THAT ALONE DOES NOT MAKE IT CORE. Sources are listed,
# not globbed, so every new board adds lines there -- its source under src/boards/, its unit
# test under tests/, a comment, and the add_test blocks for its acceptance tests. A
# CMakeLists.txt diff made only of those is code. Any other line -- a flag, an option, a
# find_package, the version, a path under src/core -- is core.
#
# Needs the base present: check out with fetch-depth: 0.
set -euo pipefail

base="${1:?usage: ci-changed-code.sh <base-commit>}"
short=$(git rev-parse --short "$base")

# Does the CMakeLists.txt diff only list non-core sources and register tests? Exit 0 if so.
# A changed line passes if it is blank, a comment, a bare non-core source path, or part of an
# add_test(...) / set_tests_properties(...) call -- followed across lines by counting parens.
# The first line that is none of these prints and makes the answer core.
cmake_lists_only_noncore() {
    local other
    other=$(git diff -U0 "$base" HEAD -- CMakeLists.txt | awk '
        /^(\+\+\+|---) / { next }
        !/^[+-]/         { depth = 0; next }      # a hunk header: a new place in the file
        {
            line = substr($0, 2)
            if (depth == 0) {
                if (line ~ /^[ \t]*(#.*)?$/) next
                if (line ~ /^[ \t]*(src\/(boards|chips|cli|mcp)|tests)\/[^ \t]+[ \t]*$/) next
                if (line !~ /^[ \t]*(add_test|set_tests_properties)[ \t]*\(/) { print; exit }
            }
            o = gsub(/\(/, "(", line); c = gsub(/\)/, ")", line)
            depth += o - c
        }')
    [ -z "$other" ] || { echo "ci-changed-code: CMakeLists.txt: $other" >&2; return 1; }
}

level=docs
why=""
while IFS= read -r f; do
    [ -z "$f" ] && continue
    case "$f" in
        docs/*|reference/*|.claude/*|LICENSE) continue ;;  # documentation
        */README.md) continue ;;          # a directory's README, at any depth, is documentation
        tools/ci-*) ;;                    # the CI scripts are core -- they decide what CI builds
        CMakeLists.txt)                   # core, unless it only lists a new non-core source
            if cmake_lists_only_noncore; then
                if [ "$level" = docs ]; then level=code; why="$f (non-core sources only)"; fi
                continue
            fi ;;
        src/boards/*|src/chips/*|src/cli/*|src/mcp/*|tests/*|machines/*|roms/*|tapes/*|disks/*|cpm/*|examples/*|tools/*|*/*.md)
            if [ "$level" = docs ]; then level=code; why="$f"; fi
            continue ;;
        *.md) continue ;;                 # a top-level .md (README, DESIGN) is documentation
        *) ;;                             # anything else is core
    esac
    echo "ci-changed-code: $f changed since $short -- core" >&2
    echo core
    exit 0
done < <(git diff --name-only "$base" HEAD)

case "$level" in
    code) echo "ci-changed-code: $why changed since $short -- code, nothing core" >&2 ;;
    docs) echo "ci-changed-code: documentation only since $short" >&2 ;;
esac
echo "$level"
