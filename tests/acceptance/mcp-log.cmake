# `--log FILE` UNDER --mcp: A TEXT TRANSCRIPT OF THE CONSOLE (issue #666).
#
# The unit suites prove the tap and the server wiring. This proves the command line, through
# the real binary:
#   * `--log` without `--mcp` is refused, and says what to use instead;
#   * a log file that cannot be opened is refused BEFORE the session starts -- a person who
#     asked for a log must not find an empty file an hour in;
#   * a session starts the file empty (what an earlier session left is gone) and writes what
#     the guest prints, as text, with nothing added.
#
# A pipe is enough: the one `run` boots a ROM monitor to its banner, which is unprompted
# output, and the server exits at EOF on stdin.
#
# Expects: -DSIM=<altairsim> -DBIN=<binary dir>

cmake_minimum_required(VERSION 3.16)

set(work "${BIN}/mcp-log-work")
file(REMOVE_RECURSE "${work}")
file(MAKE_DIRECTORY "${work}")

function(fail why)
  message(FATAL_ERROR "mcp-log: ${why}\n"
                      "--- exit ---\n${rc}\n--- stdout ---\n${out}\n--- stderr ---\n${err}")
endfunction()

# ---- 1. --log needs --mcp ----
execute_process(
  COMMAND           "${SIM}" altmon --log "${work}/never.log" -x QUIT
  WORKING_DIRECTORY "${work}"
  RESULT_VARIABLE   rc
  OUTPUT_VARIABLE   out
  ERROR_VARIABLE    err
  TIMEOUT           30
)
if(NOT rc EQUAL 2)
  fail("`--log` without `--mcp` exited ${rc}, expected 2")
endif()
if(NOT err MATCHES "--log needs --mcp")
  fail("`--log` without `--mcp` did not say that it needs --mcp")
endif()
if(EXISTS "${work}/never.log")
  fail("a refused `--log` still created its file")
endif()

# ---- 2. a log that cannot be opened is refused at startup ----
file(WRITE "${work}/in.jsonl"
  "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{}}\n"
  "{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\"}\n"
  "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"tools/call\",\"params\":{\"name\":\"run\","
  "\"arguments\":{\"from\":63488,\"until\":\"ALTMON\",\"timeout_ms\":10000}}}\n")

execute_process(
  COMMAND           "${SIM}" altmon --mcp --log "${work}/no-such-folder/session.log"
  WORKING_DIRECTORY "${work}"
  INPUT_FILE        "${work}/in.jsonl"
  RESULT_VARIABLE   rc
  OUTPUT_VARIABLE   out
  ERROR_VARIABLE    err
  TIMEOUT           30
)
if(NOT rc EQUAL 2)
  fail("an unopenable `--log` exited ${rc}, expected 2")
endif()
if(NOT err MATCHES "--log: cannot open")
  fail("an unopenable `--log` did not say that it cannot open the file")
endif()
if(NOT out STREQUAL "")
  fail("an unopenable `--log` still started the MCP session")
endif()

# ---- 3. a session starts the file empty and writes the guest's output to it ----
file(WRITE "${work}/session.log" "STALE-FROM-AN-EARLIER-RUN\n")

execute_process(
  COMMAND           "${SIM}" altmon --mcp --log session.log
  WORKING_DIRECTORY "${work}"
  INPUT_FILE        "${work}/in.jsonl"
  RESULT_VARIABLE   rc
  OUTPUT_VARIABLE   out
  ERROR_VARIABLE    err
  TIMEOUT           60
)
if(NOT rc EQUAL 0)
  fail("`altairsim altmon --mcp --log session.log` exited ${rc}, expected 0")
endif()
if(NOT EXISTS "${work}/session.log")
  fail("no session.log in the launch folder -- a relative --log path is relative to it")
endif()

file(READ "${work}/session.log" text)
if(text MATCHES "STALE")
  fail("the log was not started empty: it still holds an earlier run's text\n--- log ---\n${text}")
endif()
if(NOT text MATCHES "ALTMON")
  fail("the log does not hold the monitor's banner\n--- log ---\n${text}")
endif()
if(text MATCHES "altairsim capture")
  fail("the log has the hex capture's header: it must be the guest's text only\n--- log ---\n${text}")
endif()

message(STATUS "mcp-log: PASS")
