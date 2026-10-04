# `--mirror pty` UNDER --mcp: THE CONSOLE ON A PSEUDO-TERMINAL (issue #683).
#
# The unit suites prove the pseudo-terminal, the mirror and the server's idle pump. This
# proves the command line, through the real binary:
#   * `--mirror pty` without `--mcp` is refused, like every --mirror;
#   * on macOS and Linux a session says on STDERR where the link is -- never on stdout,
#     which is the JSON-RPC channel -- and the link is gone when the session ends;
#   * on Windows, which has no pseudo-terminal, it is refused at startup;
#   * in the monitor, CONNECT prints the mirror's name as soon as it connects.
#
# A pipe is enough: the one `run` boots a ROM monitor to its banner, which is unprompted
# output, and the server exits at EOF on stdin.
#
# Expects: -DSIM=<altairsim> -DBIN=<binary dir>

cmake_minimum_required(VERSION 3.16)

set(work "${BIN}/mcp-mirror-pty-work")
file(REMOVE_RECURSE "${work}")
file(MAKE_DIRECTORY "${work}")

function(fail why)
  message(FATAL_ERROR "mcp-mirror-pty: ${why}\n"
                      "--- exit ---\n${rc}\n--- stdout ---\n${out}\n--- stderr ---\n${err}")
endfunction()

# ---- 1. --mirror pty needs --mcp ----
execute_process(
  COMMAND           "${SIM}" altmon --mirror pty -x QUIT
  WORKING_DIRECTORY "${work}"
  RESULT_VARIABLE   rc
  OUTPUT_VARIABLE   out
  ERROR_VARIABLE    err
  TIMEOUT           30
)
if(NOT rc EQUAL 2)
  fail("`--mirror pty` without `--mcp` exited ${rc}, expected 2")
endif()
if(NOT err MATCHES "--mirror needs --mcp")
  fail("`--mirror pty` without `--mcp` did not say that it needs --mcp")
endif()

# ---- 2. a session ----
file(WRITE "${work}/in.jsonl"
  "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{}}\n"
  "{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\"}\n"
  "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"tools/call\",\"params\":{\"name\":\"run\","
  "\"arguments\":{\"from\":63488,\"until\":\"ALTMON\",\"timeout_ms\":10000}}}\n")

execute_process(
  COMMAND           "${SIM}" altmon --mcp --mirror "pty:${work}/con"
  WORKING_DIRECTORY "${work}"
  INPUT_FILE        "${work}/in.jsonl"
  RESULT_VARIABLE   rc
  OUTPUT_VARIABLE   out
  ERROR_VARIABLE    err
  TIMEOUT           60
)

if(CMAKE_HOST_WIN32)
  if(NOT rc EQUAL 2)
    fail("`--mirror pty` on Windows exited ${rc}, expected 2")
  endif()
  if(NOT err MATCHES "not available on Windows")
    fail("`--mirror pty` on Windows did not say that it is not available")
  endif()
  if(NOT err MATCHES "socket:PORT")
    fail("the refusal did not name the sink that works on Windows")
  endif()
else()
  if(NOT rc EQUAL 0)
    fail("the session exited ${rc}, expected 0")
  endif()
  if(NOT err MATCHES "--mirror: open [^\n]*/con \\(/dev/")
    fail("stderr did not say where the link is")
  endif()
  if(out MATCHES "--mirror: open")
    fail("the note went to stdout, which is the JSON-RPC channel")
  endif()
  if(NOT out MATCHES "ALTMON")
    fail("the assistant did not read the banner through the mirror")
  endif()
  if(EXISTS "${work}/con" OR IS_SYMLINK "${work}/con")
    fail("the link was left behind after the session ended")
  endif()
endif()

# ---- 3. the monitor: CONNECT says where the mirror is, at once ----
if(NOT CMAKE_HOST_WIN32)
  # An empty stdin: the built-in's startup RUN ends at end of input, not at a timeout.
  file(WRITE "${work}/empty" "")
  execute_process(
    COMMAND           "${SIM}" altmon -x "CONNECT sio0:a console|pty:${work}/mon"
    WORKING_DIRECTORY "${work}"
    INPUT_FILE        "${work}/empty"
    RESULT_VARIABLE   rc
    OUTPUT_VARIABLE   out
    ERROR_VARIABLE    err
    TIMEOUT           30
  )
  if(NOT out MATCHES "connected to console\\|pty:[^\n]*/mon\nmirror: [^\n]*/mon \\(/dev/")
    fail("CONNECT did not print the mirror's name after it connected")
  endif()
  if(EXISTS "${work}/mon" OR IS_SYMLINK "${work}/mon")
    fail("the monitor left the link behind at exit")
  endif()
endif()

message(STATUS "mcp-mirror-pty: PASS")
