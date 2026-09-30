# THE SIMULATOR'S OWN MACHINES, BOOTED FROM A FOLDER COPIED OUT OF THE TREE.
#
# Each fixture under tests/media/<name>/ is a machine file with the media it mounts lying
# beside it. This test COPIES those folders out of the repository and boots them from there,
# so a path in a machine file only works if it resolves against the FILE -- never against the
# repo root, which is where every other test runs and where a repo-root-relative path looks
# like it works. It also covers the boards whose only whole-machine proof is a boot: the
# 88-HDSK, the Turnkey Module, the 88-UIO, the FDC+'s 1.5 MB floppy and CADzilla.
#
# These are test fixtures, not the examples: examples/ is tested only to prove the examples
# work (examples.cmake), and nothing here may depend on it.
#
# Expects: -DSIM=<altairsim> -DSRC=<source dir> -DBIN=<binary dir>

set(dist "${BIN}/media-work")
file(REMOVE_RECURSE "${dist}")

# The "distribution": the binary (already built, wherever it is) plus the fixture folders,
# side by side. Note what is NOT here -- machines/, roms/, src/, the repo. If a fixture needs
# any of it, this test fails, and it should.

# ---- 1. 4K BASIC off the 88-ACR -- from its own folder, by path, and by -s. -----------------
file(COPY "${SRC}/tests/media/basic" DESTINATION "${dist}")
set(basic "${dist}/basic")

# What 4K BASIC has to print for the machine to have actually booted off the cassette.
# `TAPE OK` is the one that cannot be faked by a machine that merely started: it is the
# output of a BASIC program typed into a BASIC that read itself off a period .TAP.
function(expect_basic out why)
  foreach(want "ALTAIR BASIC" "OK" "42" "TAPE OK")
    string(FIND "${out}" "${want}" hit)
    if(hit LESS 0)
      message(FATAL_ERROR "media: ${why}\n"
                          "  '${want}' never reached the terminal.\n--- output ---\n${out}")
    endif()
  endforeach()
endfunction()

# FROM ITS OWN FOLDER: cd into the fixture's directory and name the file. `basic4k.toml` says
# MOUNT "4K BASIC Ver 3-1.tap" -- the tape lying beside it -- and here that is also the
# working directory, so this would pass even under the old cwd-relative rule. It passes
# here because the tape is where the file says it is, which is the point: the file is now
# true no matter where the directory has been moved to.
execute_process(
  COMMAND           "${SIM}" basic4k.toml
  WORKING_DIRECTORY "${basic}"
  INPUT_FILE        "${SRC}/tests/acceptance/basic4k.keys"
  OUTPUT_VARIABLE   out
  ERROR_VARIABLE    out
  TIMEOUT           60
)
expect_basic("${out}" "`cd basic && altairsim basic4k.toml` did not boot BASIC")

# ...AND BY PATH, from the top of the distribution.
#
# The same file, the same machine, from a different directory. THIS is the half that needs
# the loader to resolve against the file rather than the process: the tape is not in the
# working directory and never will be. If a machine file meant something different
# depending on where you launched it from, it would be the very trap looksLikeFile()
# refuses to walk into (core/machines.h) -- so it must not.
execute_process(
  COMMAND           "${SIM}" basic/basic4k.toml
  WORKING_DIRECTORY "${dist}"
  INPUT_FILE        "${SRC}/tests/acceptance/basic4k.keys"
  OUTPUT_VARIABLE   out
  ERROR_VARIABLE    out
  TIMEOUT           60
)
expect_basic("${out}" "`altairsim basic/basic4k.toml` from the dist root did not boot BASIC")

# ...and its .ini twin, run with -s from the dist root (#575). The script is named from
# where you launched; the tape and loader it names lie beside IT, not beside you. Before
# #575 a -s script's lines resolved against the machine's folder, so this failed while
# `DO basic/basic4k.ini` worked.
execute_process(
  COMMAND           "${SIM}" -s basic/basic4k.ini
  WORKING_DIRECTORY "${dist}"
  INPUT_FILE        "${SRC}/tests/acceptance/basic4k.keys"
  OUTPUT_VARIABLE   out
  ERROR_VARIABLE    out
  TIMEOUT           60
)
expect_basic("${out}" "`altairsim -s basic/basic4k.ini` from the dist root did not boot BASIC")

# ---- 2. THE 88-UIO -- 8K BASIC over ONE board that is a serial port AND a cassette. ----
#
# The uio fixture boots 8K BASIC 3.2 over a single 88-UIO: its 6850 serial at 0x10 (where
# 88-2SIO Port A lives) and its cassette section at 0x06 (where an 88-ACR lives). So the
# period bootstrap MITS shipped runs UNMODIFIED against a board that is two cards in one --
# proved from its own folder and by path, exactly as the basic/ fixture is.
file(COPY "${SRC}/tests/media/uio" DESTINATION "${dist}")
set(uio "${dist}/uio")

# 8K BASIC prints more than the four: its version banner and the EIGHT-K marker are the
# cheapest proof it is the 8K image read whole off the cassette, and `42`/`TAPE OK` are a
# program the interpreter ran after coming off tape.
function(expect_basic8k out why)
  foreach(want "ALTAIR BASIC VERSION 3.2" "[EIGHT-K VERSION]" "OK" " 42" "TAPE OK")
    string(FIND "${out}" "${want}" hit)
    if(hit LESS 0)
      message(FATAL_ERROR "media: ${why}\n"
                          "  '${want}' never reached the terminal.\n--- output ---\n${out}")
    endif()
  endforeach()
endfunction()

execute_process(
  COMMAND           "${SIM}" uio.toml
  WORKING_DIRECTORY "${uio}"
  INPUT_FILE        "${SRC}/tests/acceptance/basic8k.keys"
  OUTPUT_VARIABLE   out
  ERROR_VARIABLE    out
  TIMEOUT           60
)
expect_basic8k("${out}" "`cd uio && altairsim uio.toml` did not boot 8K BASIC over the 88-UIO")

execute_process(
  COMMAND           "${SIM}" uio/uio.toml
  WORKING_DIRECTORY "${dist}"
  INPUT_FILE        "${SRC}/tests/acceptance/basic8k.keys"
  OUTPUT_VARIABLE   out
  ERROR_VARIABLE    out
  TIMEOUT           60
)
expect_basic8k("${out}" "`altairsim uio/uio.toml` from the dist root did not boot 8K BASIC")

# ---- 3. ONE BASE: TYPED PATHS AND THE MACHINE FILE'S OWN RESOLVE TOGETHER. --------------
#
# A path a HUMAN TYPES resolves against the MACHINE's directory -- the very folder the
# machine file's own `mount =` names -- not against whatever shell they launched from. So
# the identical MOUNT, whether it sits in the startup list or is typed at the prompt, finds
# the SAME tape beside the machine file, from wherever you ran altairsim. (This is the
# unification: typed paths used to go to the shell's cwd, which is how the same disk could
# show under two different names. hostdir is the one base that stays separate.)
#
# So: a machine file whose startup mounts the tape BESIDE IT, followed by the identical
# MOUNT typed at the prompt from a directory that is NOT the machine's -- and BOTH must
# succeed, resolving to the tape beside the machine file. The two commands are
# character-for-character the same, and now so is where they look.
file(WRITE "${basic}/probe.toml"
     "[machine]\nname = \"probe\"\nbase = \"basic4k\"\n"
     "startup = [\"MOUNT acr0:tape \\\"4K BASIC Ver 3-1.tap\\\"\"]\n")

execute_process(
  COMMAND           "${SIM}" basic/probe.toml -x "MOUNT acr0:tape \"4K BASIC Ver 3-1.tap\""
  WORKING_DIRECTORY "${dist}"
  OUTPUT_VARIABLE   out
  ERROR_VARIABLE    out
  TIMEOUT           30
)

# Both resolved against the machine's directory, which lives beside the tape -- so both
# narrate the same path. The startup one proves the file rule; the typed one, run from
# ${dist} and not from basic/, proves a typed path now roots there too.
string(FIND "${out}" "mounted basic/4K BASIC Ver 3-1.tap" hit)
if(hit LESS 0)
  message(FATAL_ERROR
    "media: the STARTUP mount did not resolve against the machine file's directory.\n"
    "--- output ---\n${out}")
endif()

# The typed one must NOT have failed -- if it resolved against ${dist} (the old cwd rule)
# it would say "no such file". One base means it found the tape beside the machine file.
string(FIND "${out}" "no such file" miss)
if(miss GREATER -1)
  message(FATAL_ERROR
    "media: A TYPED PATH DID NOT RESOLVE AGAINST THE MACHINE'S DIRECTORY.\n"
    "  `MOUNT acr0:tape \"4K BASIC Ver 3-1.tap\"` was typed from ${dist} and should have\n"
    "  found the tape beside the machine file, as its own startup does. It did not --\n"
    "  the one-base rule (typed paths root at the machine's directory) has regressed.\n"
    "--- output ---\n${out}")
endif()

# ---- 4. THE FDC+'s 1.5 MB FLOPPY -- a disk the boot PROM cannot read, booted anyway. ----
#
# cpm22-fdcplus-hdf.toml puts CPM22-48K-HDF.dsk in an FDC+ at drive type 5 and boots it with
# the stock DBL, which knows nothing of 10,240-byte tracks. The card's firmware hands DBL a
# fake Altair sector of its own, whose loader reads the real track 0 -- so the banner alone
# proves the fake sector, the loader's no-handshake track read at 2 MHz, and the BIOS after
# it; `A: ASM      COM` is the directory read off the image. (The cpm/ folder also holds the
# 8" floppy the Turnkey fixture below borrows.)
file(COPY "${SRC}/tests/media/cpm" DESTINATION "${dist}")
set(cpm "${dist}/cpm")

function(expect_hdf out why)
  foreach(want "48K CP/M 2.2b v1.2" "For Altair 1.5Mb Floppy" "A>" "A: ASM      COM")
    string(FIND "${out}" "${want}" hit)
    if(hit LESS 0)
      message(FATAL_ERROR "media: ${why}\n"
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
expect_hdf("${out}" "`cd cpm && altairsim cpm22-fdcplus-hdf.toml` did not boot CP/M")

# ---- 5. THE HARD DISK -- CP/M booted through the 88-HDSK Datakeeper controller. --------
#
# Same shape as the BASIC fixture above: the disk is beside the machine file, booted from its
# own directory and by path. It is also the whole-machine proof of the board -- the
# whole 88-HDSK command/handshake controller and its (cyl,side,sector) mapping, exercised
# by HDBL loading the boot pages and by CP/M reading the directory off the platter.
#
# One directory line is enough to be a real claim: `A: BOOT     ASM` is an entry read off
# the image through the controller, and DIR stops after one line because a CR is already
# waiting in the pipe (CP/M's DIR polls the console between lines, and any key aborts it).
file(COPY "${SRC}/tests/media/hdsk" DESTINATION "${dist}")
set(hdsk "${dist}/hdsk")

function(expect_hdsk out why)
  foreach(want "HDBL 2.00" "48K CP/M 2.2b v1.6" "For MITS 88-HDSK" "A0>" "A: BOOT     ASM")
    string(FIND "${out}" "${want}" hit)
    if(hit LESS 0)
      message(FATAL_ERROR "media: ${why}\n"
                          "  '${want}' never reached the terminal.\n--- output ---\n${out}")
    endif()
  endforeach()
endfunction()

execute_process(
  COMMAND           "${SIM}" hdsk.toml
  WORKING_DIRECTORY "${hdsk}"
  INPUT_FILE        "${SRC}/tests/acceptance/hdsk-dir.keys"
  OUTPUT_VARIABLE   out
  ERROR_VARIABLE    out
  TIMEOUT           60
)
expect_hdsk("${out}" "`cd hdsk && altairsim hdsk.toml` did not boot CP/M off the hard disk")

# ...and by path from the distribution root, where the platter is NOT.
execute_process(
  COMMAND           "${SIM}" hdsk/hdsk.toml
  WORKING_DIRECTORY "${dist}"
  INPUT_FILE        "${SRC}/tests/acceptance/hdsk-dir.keys"
  OUTPUT_VARIABLE   out
  ERROR_VARIABLE    out
  TIMEOUT           60
)
expect_hdsk("${out}" "`altairsim hdsk/hdsk.toml` from the dist root did not boot CP/M")

# ---- 6. THE 8800bt -- THE SAME CP/M, BOOTED BY THE TURNKEY MODULE. ----------------------
#
# The turnkey fixture is the front-panel-less 8800b: one card (the Systems Turnkey Module)
# carries the boot PROM, the 6850 console at 0x10, the sense switches at FF, and the
# Auto-Start circuit. floppy.toml and hdsk.toml are deltas on the built-in `turnkey`
# machine that borrow the images from the cpm and hdsk directories copied above -- so this
# proves the WHOLE card end to end: `RUN 0000` jams `JMP` onto the bus, DBL/HDBL runs out
# of the phantom PROM, and the top 1K of the machine's 64K becomes RAM once the PROM
# switches itself out. The two directories above must already be in ${dist} for the
# ../cpm and ../hdsk mounts to resolve.
file(COPY "${SRC}/tests/media/turnkey" DESTINATION "${dist}")
set(turnkey "${dist}/turnkey")

function(expect_contains out why)
  math(EXPR last "${ARGC} - 1")
  foreach(i RANGE 2 ${last})
    string(FIND "${out}" "${ARGV${i}}" hit)
    if(hit LESS 0)
      message(FATAL_ERROR "media: ${why}\n"
                          "  '${ARGV${i}}' never reached the terminal.\n--- output ---\n${out}")
    endif()
  endforeach()
endfunction()

# Floppy: DBL, jammed at reset out of the phantom PROM, to 56K CP/M.
execute_process(
  COMMAND           "${SIM}" floppy.toml
  WORKING_DIRECTORY "${turnkey}"
  INPUT_FILE        "${SRC}/tests/acceptance/hdsk-dir.keys"
  OUTPUT_VARIABLE   out
  ERROR_VARIABLE    out
  TIMEOUT           60
)
expect_contains("${out}" "`cd turnkey && altairsim floppy.toml` did not boot CP/M off the floppy"
                "56K CP/M 2.2b" "For Altair 8" "A>")

# Hard disk: HDBL from socket L1, the Auto-Start switches moved to FC00, to 48K CP/M.
execute_process(
  COMMAND           "${SIM}" hdsk.toml
  WORKING_DIRECTORY "${turnkey}"
  INPUT_FILE        "${SRC}/tests/acceptance/hdsk-dir.keys"
  OUTPUT_VARIABLE   out
  ERROR_VARIABLE    out
  TIMEOUT           60
)
expect_contains("${out}" "`cd turnkey && altairsim hdsk.toml` did not boot CP/M off the hard disk"
                "HDBL 2.00" "48K CP/M 2.2b v1.6" "For MITS 88-HDSK" "A0>")

# ---- 7. CADZILLA -- drawdemo under CP/M, off the fixture's own floppy. ------------------
#
# The cadzilla fixture boots CP/M from cpm22b23-56k-drawdemo.dsk and types DRAWDEMO.
# Driven over --mcp, not piped keys: drawdemo waits for a key between screens, and a key that
# arrives while it is still printing is taken by the BDOS's ^S check and lost (it prints with
# function 9 and reads with function 6) -- so each key must wait for the whole prompt, which
# only `until` can do. MCP does not run the machine file's startup, so the boot is `from` FF00.
#
# What it proves: the disk boots and carries the program (a machine that merely started gets
# no banner), drawdemo started the board at 1024x768 with the wiring right (SHOW cad0 -- a
# program that never touched the ACRTC leaves the video off), and ESC takes it back to A>.
file(COPY "${SRC}/tests/media/cadzilla" DESTINATION "${dist}")
set(cz "${dist}/cadzilla")

file(WRITE "${dist}/drawdemo.jsonl" [=[
{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"media","version":"1"}}}
{"jsonrpc":"2.0","method":"notifications/initialized"}
{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"run","arguments":{"from":65280,"until":"A>","timeout_ms":30000}}}
{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"run","arguments":{"input":"DRAWDEMO\r","until":"quits)... ","timeout_ms":30000}}}
{"jsonrpc":"2.0","id":4,"method":"tools/call","params":{"name":"run","arguments":{"input":" ","until":"quits)... ","timeout_ms":30000}}}
{"jsonrpc":"2.0","id":5,"method":"tools/call","params":{"name":"monitor","arguments":{"command":"SHOW cad0"}}}
{"jsonrpc":"2.0","id":6,"method":"tools/call","params":{"name":"monitor","arguments":{"command":"SHOW cpu0"}}}
{"jsonrpc":"2.0","id":7,"method":"tools/call","params":{"name":"run","arguments":{"input":"\u001b","until":"A>","timeout_ms":30000}}}
]=])

function(expect_drawdemo toml wants)
  execute_process(
    COMMAND           "${SIM}" ${toml} --mcp
    WORKING_DIRECTORY "${cz}"
    INPUT_FILE        "${dist}/drawdemo.jsonl"
    OUTPUT_VARIABLE   out
    ERROR_VARIABLE    err
    TIMEOUT           60
  )
  foreach(want "CADzilla drawdemo: the ACRTC drawing commands, 1024x768"
               " 1/21  DOT"
               " 2/21  ALINE"
               "video            on"
               "picture          1024x768 at (0,0)"
               "wiring           ok"
               "drawdemo done."
               ${wants})
    string(FIND "${out}" "${want}" hit)
    if(hit LESS 0)
      message(FATAL_ERROR "media: `altairsim ${toml} --mcp` did not run drawdemo.\n"
                          "  '${want}' never came back.\n--- stdout ---\n${out}\n--- stderr ---\n${err}")
    endif()
  endforeach()
  string(FIND "${out}" "ACRTC command error" hit)
  if(hit GREATER_EQUAL 0)
    message(FATAL_ERROR "media: drawdemo reported an ACRTC command error under ${toml}.\n--- stdout ---\n${out}")
  endif()
endfunction()

# The full-speed machine, and the real-speed one beside it. drawdemo-real.toml names
# `base = "drawdemo.toml"`, so this also proves a relative base resolves in the copied folder.
expect_drawdemo(drawdemo.toml      "draw_rate        full;clock_hz         0 ")
expect_drawdemo(drawdemo-real.toml "draw_rate        real;clock_hz         2000000")

file(REMOVE_RECURSE "${dist}")
message(STATUS "media: the fixture machines boot from their own folders, and a typed path "
               "resolves against the machine's folder.")
