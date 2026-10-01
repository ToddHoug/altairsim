# NO TEST OF THE SIMULATOR MAY READ examples/.
#
# examples/ is the product: what a user is handed, tested only to prove the example itself
# works (examples.cmake). The simulator's tests take their machines and media from tests/media,
# so an example can change, move or leave the repository without a board test going red -- and
# a board test can never pass because an example happened to be lying around. That line held
# only by habit once, and the habit failed: the tests had grown to lean on sixteen examples.
#
# So this reads every test script, fixture and source file, and CMakeLists.txt, and fails on a
# line of CODE that names examples/. Comments may talk about the examples; code may not use
# them. The few files whose job IS the shipped tree are allowed by name.
#
# Expects: -DSRC=<source dir>

cmake_minimum_required(VERSION 3.20)

# The files that exist to check the shipped tree itself: the examples, the package manifest,
# and the manual's promise of what the package holds. And this file, which has to say the word.
set(allowed
    tests/acceptance/examples.cmake
    tests/acceptance/docs-package.cmake
    tests/acceptance/docs-manual.cmake
    tests/acceptance/no-example-deps.cmake)

file(GLOB_RECURSE files RELATIVE "${SRC}"
     "${SRC}/tests/*.cmake" "${SRC}/tests/*.exp" "${SRC}/tests/*.toml" "${SRC}/tests/*.ini"
     "${SRC}/tests/*.cmd" "${SRC}/tests/*.sh" "${SRC}/tests/*.cpp" "${SRC}/tests/*.h"
     "${SRC}/tests/*.keys")
list(APPEND files CMakeLists.txt)

set(hits "")
set(checked 0)
foreach(f ${files})
  if(f IN_LIST allowed)
    continue()
  endif()
  math(EXPR checked "${checked} + 1")
  file(STRINGS "${SRC}/${f}" lines)
  set(n 0)
  foreach(line IN LISTS lines)
    math(EXPR n "${n} + 1")
    # A whole-line comment in any of the languages above: #, // or ; (a .cmd/.ini script).
    if(line MATCHES "^[ \t]*(#|//|;)")
      continue()
    endif()
    if(line MATCHES "examples/")
      string(APPEND hits "  ${f}:${n}: ${line}\n")
    endif()
  endforeach()
endforeach()

if(checked LESS 50)
  message(FATAL_ERROR "examples-isolation: only ${checked} files were read -- the glob is wrong, "
                      "and a guard that reads nothing passes on everything.")
endif()

if(NOT hits STREQUAL "")
  message(FATAL_ERROR
    "examples-isolation: a test of the simulator reads examples/.\n"
    "  Give the test its own copy under tests/media/<name>/ and point it there -- an example is\n"
    "  tested only to prove the example works (tests/acceptance/examples.cmake).\n${hits}")
endif()

message(STATUS "examples-isolation: ${checked} files read; no test of the simulator uses examples/.")
