# W NAMES THE HOST FILE WITHOUT THE CP/M ATTRIBUTE BITS -- ON CP/M 3, WHERE OPEN HANDS THEM BACK.
#
# CP/M keeps a file's R/O, SYSTEM and ARCHIVE flags in the high bit of three of its name
# characters. W builds the host file's name from its FCB, and it does so AFTER opening the file
# -- and CP/M 3's OPEN writes those flags back into the caller's FCB. So a read-only HELP.COM
# came out as the host name "HELP.<0C3H>OM", which no host will create: W said "cannot open for
# writing" and copied nothing. CP/M 2.2's OPEN leaves the FCB alone, which is why the CP/M 2.2
# test never saw it.
#
# The banked SD Systems CP/M 3 master ships HELP.COM with an attribute bit set, so this boots it,
# runs the COMMITTED cpm/hostbridge/W.COM (not whatever copy is on the disk) as `W HELP.COM`, and
# checks the host got a file called exactly HELP.COM.
#
# Getting W.COM into the TPA of a BANKED machine: at the A> prompt the system bank is selected,
# so 0100H there is not where a program runs. We let the CCP launch a program that is on the
# disk (HEXCOM) with a breakpoint at 0100H; when it fires the TPA bank is live and page zero is
# built, and W.COM is loaded over HEXCOM before it runs an instruction.
#
# Expects: -DSIM=<altairsim> -DSRC=<source dir> -DBIN=<binary dir>

cmake_minimum_required(VERSION 3.20)

set(work "${BIN}/hostbridge-attrs")
file(REMOVE_RECURSE "${work}")
file(MAKE_DIRECTORY "${work}/host")
foreach(f cpm3-b.toml CPM3-B-SSDD-60K-DISK1.DSK CPM3-B-SSDD-DISK2.DSK)
  configure_file("${SRC}/examples/sdsys/${f}" "${work}/${f}" COPYONLY)
endforeach()

# 005CH: the FCB the CCP would have parsed for `W HELP.COM` (drive 0, name, type, zeros), with
# the second FCB at 006CH blank. 0080H: the tail, as length + text.
set(fcb "00 48 45 4C 50 20 20 20 20 43 4F 4D 00 00 00 00 00 20 20 20 20 20 20 20 20 20 20 20 00 00 00 00")
set(tail "09 20 48 45 4C 50 2E 43 4F 4D 00")

set(n 0)
set(rpc "")
function(call tool args)
  math(EXPR id "${n} + 1")
  set(n "${id}" PARENT_SCOPE)
  string(CONCAT line "${rpc}{\"jsonrpc\":\"2.0\",\"id\":${id},\"method\":\"tools/call\","
                     "\"params\":{\"name\":\"${tool}\",\"arguments\":${args}}}\n")
  set(rpc "${line}" PARENT_SCOPE)
endfunction()

string(APPEND rpc
  "{\"jsonrpc\":\"2.0\",\"id\":0,\"method\":\"initialize\",\"params\":{}}\n"
  "{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\"}\n")
call(board_set   "{\"id\":\"hb0\",\"key\":\"hostdir\",\"value\":\"${work}/host\"}")
call(run         "{\"from\":57344,\"input\":\"\\r\",\"until\":\".\",\"timeout_ms\":60000}")
call(run         "{\"input\":\"C\\r\",\"until\":\"A>\",\"timeout_ms\":60000}")
call(breakpoints "{\"action\":\"add\",\"kind\":\"pc\",\"lo\":256}")
call(run         "{\"input\":\"HEXCOM\\r\",\"timeout_ms\":60000}")
call(breakpoints "{\"action\":\"clear\"}")
call(mem_load    "{\"path\":\"${SRC}/cpm/hostbridge/W.COM\",\"at\":256,\"format\":\"BIN\"}")
call(mem_deposit "{\"addr\":92,\"bytes\":\"${fcb}\"}")
call(mem_deposit "{\"addr\":128,\"bytes\":\"${tail}\"}")
call(run         "{\"until\":\"A>\",\"timeout_ms\":60000}")
file(WRITE "${work}/in.jsonl" "${rpc}")

execute_process(
  COMMAND           "${SIM}" cpm3-b.toml --mcp
  WORKING_DIRECTORY "${work}"
  INPUT_FILE        "${work}/in.jsonl"
  OUTPUT_VARIABLE   out
  ERROR_VARIABLE    err
  TIMEOUT           120
)
file(WRITE "${work}/mcp.log" "${out}")

function(fail why)
  message(FATAL_ERROR "hostbridge-attrs: ${why}\n  transcript: ${work}/mcp.log")
endfunction()

if(NOT out MATCHES "stopped: breakpoint")
  fail("HEXCOM never reached 0100H, so W.COM was never run")
endif()
if(NOT out MATCHES "W: HELP\\.COM -> HELP\\.COM")
  fail("W did not name the host file HELP.COM -- the attribute bits leaked into the name")
endif()
file(GLOB got RELATIVE "${work}/host" "${work}/host/*")
if(NOT got STREQUAL "HELP.COM")
  fail("the host directory holds [${got}], not exactly HELP.COM")
endif()
file(SIZE "${work}/host/HELP.COM" sz)
if(sz EQUAL 0)
  fail("HELP.COM arrived empty")
endif()
message(STATUS "hostbridge-attrs: W copied a CP/M 3 file with attributes set to HELP.COM (${sz} bytes)")
