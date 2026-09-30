# THE EXAMPLES, IN THE LAYOUT A USER ACTUALLY GETS.
#
# examples/ is not a test fixture. It is what we SHIP: a user gets the `altairsim` binary and
# that tree, and nothing else -- no repository, no build directory, no tests. So this file
# does not ask "does the simulator work" -- the simulator's own tests answer that from their
# own fixtures (tests/media, media.cmake), and none of them may read examples/. It asks:
#
#     DOES THE THING WE HAND PEOPLE WORK WHERE WE HAND IT TO THEM?
#
# So it COPIES each example out of the tree, leaving the repository behind, and runs EVERY
# machine file in it from there -- the only arrangement in which a repo-root-relative path in
# an example is visible as the bug it is.
#
# Expects: -DSIM=<altairsim> -DSRC=<source dir> -DBIN=<binary dir> -DHAVE_SDL=<0|1>

set(dist "${BIN}/examples-work")
file(REMOVE_RECURSE "${dist}")

# The whole of an MCP session, sent up front: every `run` waits for its `until` before the
# next request is read, so the file is a script, not a race.
function(mcp_session file)
  set(lines [=[{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"examples","version":"1"}}}
{"jsonrpc":"2.0","method":"notifications/initialized"}]=])
  set(id 2)
  foreach(req ${ARGN})
    string(APPEND lines "\n{\"jsonrpc\":\"2.0\",\"id\":${id},\"method\":\"tools/call\",\"params\":${req}}")
    math(EXPR id "${id} + 1")
  endforeach()
  file(WRITE "${file}" "${lines}\n")
endfunction()

function(expect_contains out why)
  math(EXPR last "${ARGC} - 1")
  foreach(i RANGE 2 ${last})
    string(FIND "${out}" "${ARGV${i}}" hit)
    if(hit LESS 0)
      message(FATAL_ERROR "examples: ${why}\n"
                          "  '${ARGV${i}}' never reached the terminal.\n--- output ---\n${out}")
    endif()
  endforeach()
endfunction()

# ---- 1. examples/cpm -- THE QUICK START'S OWN MACHINE, and every other file beside it. ------
#
# `examples/cpm` is the flagship: it is what docs/manual/quick-start.md promises, and until
# 2026-07-19 NO test booted it. `acceptance-dcdd-readonly` boots a test-owned machine file
# that happens to mount the same image, which proves the CARD and proves nothing at all
# about the example -- and that is much of how the manual drifted as far as it did.
#
# The keys are cpm-dir.keys, and its first byte is a NUL for the same reason basic4k.keys's
# is: every keystroke is in the buffer before the machine is switched on, and CP/M's cold
# start clears the SIO's receive register and eats exactly one byte.
file(COPY "${SRC}/examples/cpm" DESTINATION "${dist}/examples")
set(cpm "${dist}/examples/cpm")

# WHY ONLY THE FIRST DIRECTORY LINE IS CLAIMED, and it is CP/M's behaviour, not a hedge:
# DIR polls the console between lines so the operator can stop a long listing, and ANY key
# already waiting aborts it. Our keys are all in the pipe before the machine starts, so the
# CR behind `DIR` stops the listing after one line -- every time, and on purpose, since the
# alternative is a test whose output depends on how fast the host is.
#
# One line is enough to be a real claim. `A: L80      COM` is a directory entry read off
# the image through the 88-DCDD: a machine that merely started prints the banner and stops.
function(expect_cpm out why)
  foreach(want "56K CP/M 2.2b v2.3" "For Altair 8\" Floppy" "A>" "A: L80      COM")
    string(FIND "${out}" "${want}" hit)
    if(hit LESS 0)
      message(FATAL_ERROR "examples: ${why}\n"
                          "  '${want}' never reached the terminal.\n--- output ---\n${out}")
    endif()
  endforeach()
endfunction()

execute_process(
  COMMAND           "${SIM}" cpm22-buffered.toml
  WORKING_DIRECTORY "${cpm}"
  INPUT_FILE        "${SRC}/tests/acceptance/cpm-dir.keys"
  OUTPUT_VARIABLE   out
  ERROR_VARIABLE    out
  TIMEOUT           60
)
expect_cpm("${out}" "`cd examples/cpm && altairsim cpm22-buffered.toml` did not boot CP/M")

# ...and by path from the distribution root, where the disk is NOT. Same machine, and the
# floppy has to be found beside the file that names it rather than beside the operator.
execute_process(
  COMMAND           "${SIM}" examples/cpm/cpm22-buffered.toml
  WORKING_DIRECTORY "${dist}"
  INPUT_FILE        "${SRC}/tests/acceptance/cpm-dir.keys"
  OUTPUT_VARIABLE   out
  ERROR_VARIABLE    out
  TIMEOUT           60
)
expect_cpm("${out}" "`altairsim examples/cpm/cpm22-buffered.toml` from the dist root did not boot CP/M")

# The flagship's .ini twin, by -s from the dist root: the floppy is beside the script (#575).
execute_process(
  COMMAND           "${SIM}" -s examples/cpm/cpm22-buffered.ini
  WORKING_DIRECTORY "${dist}"
  INPUT_FILE        "${SRC}/tests/acceptance/cpm-dir.keys"
  OUTPUT_VARIABLE   out
  ERROR_VARIABLE    out
  TIMEOUT           60
)
expect_cpm("${out}" "`altairsim -s examples/cpm/cpm22-buffered.ini` from the dist root did not boot CP/M")

# cpm22-fdcplus-hdf.toml -- the FDC+'s 1.5 MB floppy, a disk the boot PROM cannot read. It puts CPM22-48K-HDF.dsk in an FDC+ at drive type 5 and boots it with
# the stock DBL, which knows nothing of 10,240-byte tracks. The card's firmware hands DBL a
# fake Altair sector of its own, whose loader reads the real track 0 -- so the banner alone
# proves the fake sector, the loader's no-handshake track read at 2 MHz, and the BIOS after
# it; `A: ASM      COM` is the directory read off the image. (media.cmake proves the same boot
# on the simulator's own copy; this proves the file we ship.)
function(expect_hdf out why)
  foreach(want "48K CP/M 2.2b v1.2" "For Altair 1.5Mb Floppy" "A>" "A: ASM      COM")
    string(FIND "${out}" "${want}" hit)
    if(hit LESS 0)
      message(FATAL_ERROR "examples: ${why}\n"
                          "  '${want}' never reached the terminal.\n--- output ---\n${out}")
    endif()
  endforeach()
endfunction()

execute_process(
  COMMAND           "${SIM}" cpm22-fdcplus-hdf.toml
  WORKING_DIRECTORY "${cpm}"
  INPUT_FILE        "${SRC}/tests/acceptance/cpm-dir.keys"
  OUTPUT_VARIABLE   out
  ERROR_VARIABLE    out
  TIMEOUT           60
)
expect_hdf("${out}" "`cd examples/cpm && altairsim cpm22-fdcplus-hdf.toml` did not boot CP/M")

# cpm22-fdcplus.toml boots off an FDC+ Serial Drive Server -- a real serial port and a real
# server on the far end, which no test machine has. The file names ONE computer's port, and its
# README's step 2 is "set `connect` on fdc0 to your serial port". So the test does that step on
# the copy -- with `null`, the one endpoint every host has -- and the file is then LOADED, not
# booted: over --mcp, where the startup (the RUN that would wait on the server) is not run, and
# SHOW fdc0 proves the file built the board it says -- the drive type, the rate and the endpoint.
# (Loading the file as shipped opens its port, which passes only on a machine that has it.)
file(READ "${cpm}/cpm22-fdcplus.toml" fdcplus)
string(REGEX REPLACE "\nconnect = \"serial:[^\"\n]*\"" "\nconnect = \"null\"" fdcplus_null "${fdcplus}")
if(fdcplus_null STREQUAL fdcplus)
  message(FATAL_ERROR "examples: cpm22-fdcplus.toml has no `connect = \"serial:...\"` line on fdc0 "
                      "-- the line its README tells the reader to set.")
endif()
file(WRITE "${cpm}/cpm22-fdcplus.toml" "${fdcplus_null}")

mcp_session("${dist}/fdcplus.jsonl"
  [=[{"name":"monitor","arguments":{"command":"SHOW fdc0"}}]=])
execute_process(
  COMMAND           "${SIM}" cpm22-fdcplus.toml --mcp
  WORKING_DIRECTORY "${cpm}"
  INPUT_FILE        "${dist}/fdcplus.jsonl"
  RESULT_VARIABLE   rc
  OUTPUT_VARIABLE   out
  ERROR_VARIABLE    out
  TIMEOUT           30
)
expect_contains("${out}" "`altairsim cpm22-fdcplus.toml`, with its port set, did not load"
                "fdc0  (fdcplus)" "drivetype        7" "baud             230400"
                "connect          null")
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "examples: cpm22-fdcplus.toml exited ${rc}.\n--- output ---\n${out}")
endif()

# cpm22-terminal.toml puts the console in the built-in windowed VT100. A build with SDL loads it
# -- under SDL's dummy video driver, so no window opens on a test machine -- and the console
# line must hold the terminal. A headless build (no SDL) must REFUSE it, with the one message
# that says why: the spec parsed, and only the missing window stopped it. Each leg asserts the
# answer its own build must give; neither passes on the other's.
mcp_session("${dist}/terminal.jsonl"
  [=[{"name":"monitor","arguments":{"command":"SHOW sio0"}}]=])
execute_process(
  COMMAND           "${CMAKE_COMMAND}" -E env SDL_VIDEODRIVER=dummy
                    "${SIM}" cpm22-terminal.toml --mcp
  WORKING_DIRECTORY "${cpm}"
  INPUT_FILE        "${dist}/terminal.jsonl"
  OUTPUT_VARIABLE   out
  ERROR_VARIABLE    out
  TIMEOUT           30
)
if(HAVE_SDL)
  expect_contains("${out}" "`altairsim cpm22-terminal.toml` did not load on a build with SDL"
                  "sio0  (2sio)" "serial  terminal?emulation=vt100&size=80x24")
else()
  expect_contains("${out}" "a headless build did not refuse cpm22-terminal.toml for its missing window"
                  "terminal: this build has no window")
endif()

# ---- 2. examples/debugger -- the walkthrough, symbols and hex loaded from beside the file. ----
#
# examples/debugger is a taught exercise: a 46-byte program, its .PRN listing and its .HEX,
# and a README that walks the monitor's debugger. This runs that walkthrough non-
# interactively, from the example's own directory, so it proves the README is true: the
# disassembly names labels and operands the way it says, and the .prn/.hex resolve beside
# the machine file rather than beside the repo.
file(COPY "${SRC}/examples/debugger" DESTINATION "${dist}/examples")
set(dbg "${dist}/examples/debugger")

execute_process(
  COMMAND           "${SIM}" debugger.toml
                    -x "SYMBOLS LOAD HELLO.PRN"
                    -x "LOAD HELLO.HEX"
                    -x "DISASM START-DONE"
                    -x "DISASM PUTC 7"
                    -x "EXAMINE START"
                    -x "BREAK DONE"
                    -x "RUN"
  WORKING_DIRECTORY "${dbg}"
  OUTPUT_VARIABLE   out
  ERROR_VARIABLE    out
  TIMEOUT           30
)

# What has to be true: the files loaded from beside the machine file; the disassembly is
# symbolic (a leading label, a label operand, and the EQU-address operand that is the whole
# point of the feature); and the program actually ran and printed through the 2SIO.
foreach(want
        "12 symbol(s) from HELLO.PRN"   # the .PRN resolved beside debugger.toml
        "loaded 46 bytes"               # so did the .HEX
        "START:"                        # a program label heads its own line
        "CALL PUTC"                     # a 16-bit operand reads as a label
        "LXI SP,STACK"                  # ...and as an EQU-address -- the CALL BDOS case
        "IN 10"                         # a BYTE operand stays a number (a port is not an address)
        "HELLO, WORLD"                  # it ran, on the console the file wired up
        "stopped at 0112")              # and stopped at the DONE breakpoint
  string(FIND "${out}" "${want}" hit)
  if(hit LESS 0)
    message(FATAL_ERROR "examples: the debugger walkthrough did not behave as the README says.\n"
                        "  '${want}' never reached the terminal.\n--- output ---\n${out}")
  endif()
endforeach()

# And the byte operand that must NOT be named: IN 10 stays IN 10, never IN TTYS, because a
# port is a byte and only a 16-bit operand is an address (README section 2).
string(FIND "${out}" "IN TTYS" hit)
if(hit GREATER_EQUAL 0)
  message(FATAL_ERROR "examples: a BYTE operand was annotated as a symbol.\n"
                      "  'IN 10' read as 'IN TTYS' -- a port is not an address.\n--- output ---\n${out}")
endif()

# ...and debugger.ini, the same bench built by monitor commands. -s stops at the first line
# that fails and exits with its status, so a clean exit plus the last lines' answers is the
# whole script having run.
execute_process(
  COMMAND           "${SIM}" -s examples/debugger/debugger.ini
  WORKING_DIRECTORY "${dist}"
  RESULT_VARIABLE   rc
  OUTPUT_VARIABLE   out
  ERROR_VARIABLE    out
  TIMEOUT           30
)
expect_contains("${out}" "`altairsim -s examples/debugger/debugger.ini` did not build the bench"
                "sio0:a: connected to console" "mem0:0: ram  0000-7FFF  32K" "power cycled")
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "examples: debugger.ini exited ${rc}.\n--- output ---\n${out}")
endif()

# ---- 3. examples/ai-mcp -- the machine an AI assistant debugs over --mcp. -----------------
#
# The README's first step, as the assistant takes it: boot (MCP does not run the startup, so
# `run from` FF00), assemble HELLO.ASM, LOAD it, and run it -- which prints the bug the rest
# of the README goes on to find. `ELLO, WORLD` is the proof: the source on the disk, the
# assembler and the program all ran, and the bug is still there for the reader to fix. It runs
# on the COPY: ASM and LOAD write to the disk, and the tracked image must not move.
file(COPY "${SRC}/examples/ai-mcp" DESTINATION "${dist}/examples")
set(ai "${dist}/examples/ai-mcp")

mcp_session("${dist}/ai.jsonl"
  [=[{"name":"run","arguments":{"from":65280,"until":"A>","timeout_ms":30000}}]=]
  [=[{"name":"run","arguments":{"input":"ASM HELLO\r","until":"END OF ASSEMBLY","timeout_ms":30000}}]=]
  [=[{"name":"run","arguments":{"until":"A>","timeout_ms":30000}}]=]
  [=[{"name":"run","arguments":{"input":"LOAD HELLO\r","until":"RECORDS WRITTEN","timeout_ms":30000}}]=]
  [=[{"name":"run","arguments":{"until":"A>","timeout_ms":30000}}]=]
  [=[{"name":"run","arguments":{"input":"HELLO\r","until":"WORLD","timeout_ms":30000}}]=])
execute_process(
  COMMAND           "${SIM}" cpm-ai.toml --mcp
  WORKING_DIRECTORY "${ai}"
  INPUT_FILE        "${dist}/ai.jsonl"
  OUTPUT_VARIABLE   out
  ERROR_VARIABLE    out
  TIMEOUT           60
)
expect_contains("${out}" "the ai-mcp README's first step did not go as it says"
                "56K CP/M 2.2b v2.3" "END OF ASSEMBLY" "FIRST ADDRESS 0100" "ELLO, WORLD")

# ...and cpm-ai.ini, by -s from the dist root: the disk is beside the script (#575).
execute_process(
  COMMAND           "${SIM}" -s examples/ai-mcp/cpm-ai.ini
  WORKING_DIRECTORY "${dist}"
  INPUT_FILE        "${SRC}/tests/acceptance/cpm-dir.keys"
  OUTPUT_VARIABLE   out
  ERROR_VARIABLE    out
  TIMEOUT           60
)
expect_cpm("${out}" "`altairsim -s examples/ai-mcp/cpm-ai.ini` from the dist root did not boot CP/M")

file(REMOVE_RECURSE "${dist}")
message(STATUS "examples: every machine file in the shipped examples works from its own folder.")
