# tests/

This directory holds the tests and most of the media that the tests read. A small number of
acceptance tests read period media from [`tapes/`](../tapes/README.md) and
[`disks/`](../disks/README.md). A fresh clone runs every test with no download.

## Run the tests

```sh
./build/altair_tests <names>        # the unit suites that you name
./build/altair_tests --list         # the suite names
ctest --test-dir build -LE slow     # all tests except the slow processor tests
ctest --test-dir build              # all tests
```

Run the unit suites for the subsystem that you changed. CI runs the full set on Linux, macOS
and Windows for each push. A suite name that does not exist is an error.

Read the pass line, `100% tests passed out of N`. Do not judge a run by the absence of the
word "error".

## What is here

| Item | What it holds |
|---|---|
| `test_*.cpp` | The unit suites. Each file tests one subsystem or one board, and all of them build into `altair_tests`. |
| `main.cpp`, `test.h` | The test runner and the `CHECK` macros. The project uses no test framework. |
| `framecheck.h`, `framecheck.cpp` | `CHECK_FRAME`, which compares the picture that a video board drew with a text grid. No window is necessary. |
| `acceptance/` | The acceptance tests. Each one starts a whole machine through the real command line, boots period software and reads what the console shows. |
| `media/` | The disks, tapes and machine files that the acceptance tests use, in one folder for each machine or subject. |
| `cpu/` | Period test programs for the 8080, the 8085 and the Z80, with their provenance. `cpu/z80/` has its own licence. |
| `cputest.cpp` | `altair_cputest`, which runs the programs in `cpu/`. These are the slow tests. |
| `programs/` | Small 8080 programs that tests load, as source, listing and Intel HEX. |
| `golden/` | Expected output that a test compares against. |
| `boards/lamp/` | The example board from the Developer Guide. A test compiles it, so that the tutorial stays correct. |
| `serialtest.cpp` | A test of two real serial ports with a null-modem cable. It runs only when you set `ALTAIR_SERIAL_A` and `ALTAIR_SERIAL_B`. |
| `sockettest.cpp` | A test of the socket layer on the real network stack. |

### The files in `acceptance/`

| File type | What it is |
|---|---|
| `.exp` | An `expect` script. It types at the guest on a real pseudo-terminal. |
| `.cmake` | A CMake script that runs the program and checks its output, or checks the documents. |
| `.keys`, `.cmd` | Keystrokes or monitor commands that a test sends. |
| `.toml` | A machine file for one test. |

`CMakeLists.txt` at the root registers each test with `add_test`.

## Rules

- **A test proves the code.** When a test finds a bug, fix the code. Do not loosen the
  assertion, skip the test, change the input or add a retry.
- **Prove that a new test fails without the fix.** Break the fix, see the test fail, then
  restore the fix.
- **Put the media for a new test in `tests/media/`.** A test does not read `examples/`. The
  8K BASIC and Programming System II tests read their tapes from `tapes/`, and the minidisk
  tests read their disk from `disks/mits-88mds/`.
- **A test does not depend on local hardware.** A machine file that names a serial device
  passes only on the computer that has the device.
- **Normalize path separators before you compare a path.** Windows gives `\`.
- **Use `expect` only for a committed acceptance test.** To type at a guest while you work,
  use `altairsim <machine> --mcp`.

## Read more

- [Building it](../docs/devguide/building.md) in the Developer Guide, for the test targets.
- [`CONTRIBUTING.md`](../CONTRIBUTING.md).
