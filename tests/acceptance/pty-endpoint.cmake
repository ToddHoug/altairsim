# THE `pty` ENDPOINT: A SERIAL LINE ON A PSEUDO-TERMINAL (issue #685).
#
# The unit suite (`altair_tests pty`) proves the line: the pins, the bytes, the link.
# This proves the command line and the machine file, through the real binary:
#   * CONNECT prints where the line is as soon as it connects, and the link is gone
#     when the program exits;
#   * a machine file that names the line with a relative path gets the link in ITS
#     folder, and the program says where it is with no CONNECT typed;
#   * on Windows, which has no pseudo-terminal, CONNECT is refused by name -- not as an
#     unknown endpoint -- and the exit code says that a command failed.
#
# A pipe is enough: nothing here types at a guest. Each run has an EMPTY stdin, so the
# built-in's startup RUN ends at end of input and not at a timeout.
#
# Expects: -DSIM=<altairsim> -DBIN=<binary dir>

cmake_minimum_required(VERSION 3.16)

set(work "${BIN}/pty-endpoint-work")
file(REMOVE_RECURSE "${work}")
file(MAKE_DIRECTORY "${work}/machine")
file(WRITE "${work}/empty" "")

function(fail why)
  message(FATAL_ERROR "pty-endpoint: ${why}\n"
                      "--- exit ---\n${rc}\n--- stdout ---\n${out}\n--- stderr ---\n${err}")
endfunction()

# ---- 1. CONNECT, and save the machine ----
#
# The second serial unit: the first one holds the console, and this machine keeps it.
execute_process(
  COMMAND           "${SIM}" altmon -x "CONNECT sio0:b pty:line"
                             -x "CONFIG SAVE m.toml" -x QUIT
  WORKING_DIRECTORY "${work}/machine"
  INPUT_FILE        "${work}/empty"
  RESULT_VARIABLE   rc
  OUTPUT_VARIABLE   out
  ERROR_VARIABLE    err
  TIMEOUT           30
)

if(CMAKE_HOST_WIN32)
  if(rc EQUAL 0)
    fail("CONNECT ... pty on Windows exited 0, expected a failed command")
  endif()
  if(NOT out MATCHES "pty is not available on Windows")
    fail("CONNECT ... pty on Windows did not say that it is not available")
  endif()
  if(NOT out MATCHES "socket:PORT")
    fail("the refusal did not name the endpoint that works on Windows")
  endif()
  if(out MATCHES "no endpoint 'pty")
    fail("pty was refused as an unknown endpoint, not by name")
  endif()
  message(STATUS "pty-endpoint: PASS (refused, as it must be here)")
  return()
endif()

if(NOT rc EQUAL 0)
  fail("the session exited ${rc}, expected 0")
endif()
if(NOT out MATCHES "sio0:b: connected to pty:line\npty: [^\n]*line \\(/dev/")
  fail("CONNECT did not print where the line is after it connected")
endif()
if(EXISTS "${work}/machine/line" OR IS_SYMLINK "${work}/machine/line")
  fail("the link was left behind at exit")
endif()
if(NOT EXISTS "${work}/machine/m.toml")
  fail("CONFIG SAVE did not write the machine file")
endif()
file(READ "${work}/machine/m.toml" toml)
if(NOT toml MATCHES "\"pty:line\"")
  fail("the saved machine does not name the line as it was typed\n--- m.toml ---\n${toml}")
endif()

# ---- 2. the machine file, loaded from ANOTHER folder ----
#
# The link belongs in the machine file's folder, not in the folder the program started
# in -- and with no CONNECT typed, the program must still say where the line is.
execute_process(
  COMMAND           "${SIM}" machine/m.toml -x "SHOW sio0" -x QUIT
  WORKING_DIRECTORY "${work}"
  INPUT_FILE        "${work}/empty"
  RESULT_VARIABLE   rc
  OUTPUT_VARIABLE   out
  ERROR_VARIABLE    err
  TIMEOUT           30
)
if(NOT rc EQUAL 0)
  fail("loading the saved machine exited ${rc}, expected 0")
endif()
if(NOT out MATCHES "sio0:b: pty: machine/line \\(/dev/")
  fail("the loaded machine did not say where the line is")
endif()
if(EXISTS "${work}/line" OR IS_SYMLINK "${work}/line")
  fail("the link was made in the start folder, not in the machine file's folder")
endif()
if(EXISTS "${work}/machine/line" OR IS_SYMLINK "${work}/machine/line")
  fail("the link was left behind at exit")
endif()

message(STATUS "pty-endpoint: PASS")
