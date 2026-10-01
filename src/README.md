# src/

This directory holds the source of the simulator. All of it except `main.cpp` builds into one
library, `altair_core`. The program `altairsim` and the unit tests both link that library.

## What is here

The table goes from the inside of the machine to the outside.

| Folder | What it holds |
|---|---|
| `core/` | The machine itself: the S-100 bus, the `Board` base class, the clock, the `Machine` that owns the boards, and the debugger. It also holds the loaders for Intel HEX, symbols and state files, and the tables of built-in machines and ROMs. |
| `cpu/` | The processors: 8080, 8085 and Z80. |
| `isa/` | The instruction sets. A disassembler here takes bytes and returns text. It has no registers and no bus. |
| `chips/` | One class for each integrated circuit: the UARTs, the timers, the interrupt controller, the floppy controller chip and others. Each one is modelled from its data sheet. |
| `boards/` | One class for each S-100 board. A board decodes addresses and ports on the bus and uses the chips that the real board carried. |
| `host/` | The connection between a board and the host computer: byte streams, endpoints (`socket:`, `telnet:`, `serial:`, `printer:`), the console and its filters, disk and tape media, and the display and joystick. `host/terminal/` is the built-in terminal emulator. |
| `platform/` | Everything that depends on the operating system. Each header here has no conditionals. `posix/` and `win32/` each hold one implementation, and CMake selects one. |
| `cli/` | The monitor: the command table, the commands and the line editor. |
| `config/` | The TOML loader and writer for machine files. |
| `mcp/` | The MCP server, which lets an AI assistant drive a machine. |
| `util/` | A small JSON value type, for MCP. |
| `main.cpp` | The command line. It builds a machine and starts the monitor or the MCP server. |

## Rules

- **Operating-system code goes in `platform/` only.** A lint in the build searches for
  `_WIN32`, `__APPLE__`, `__linux__` and the operating-system headers in every other folder.
  The build fails if it finds one.
- **A chip is one class, and a board is another.** Model a chip from its data sheet and a
  board from its manual. Do not copy the subset that one BIOS uses.
- **Do not give hardware a behavior that it never had.** When guest software shows a symptom,
  look in the host layer, the console filters and the monitor first.
- **A board has no separate schema.** The properties of a board are its TOML keys, its `SET`
  and `SHOW` names and its MCP fields.
- **No dependencies.** The source needs a C++20 compiler and nothing else. SDL3 is optional.
  The files with `_sdl` in the name build only when CMake finds it, and the `_null` files
  replace them when it does not.
- **List a new `.cpp` file in `CMakeLists.txt`.** CMake does not search for source files.

## Read more

- [`DESIGN.md`](../DESIGN.md). Read the section for a subsystem before you change it.
- [Theory of operation](../docs/devguide/theory.md) and
  [Writing a board](../docs/devguide/adding-a-board.md) in the Developer Guide.
- [Serial I/O](../docs/devguide/serial-io.md) for the three layers between a board and a host
  endpoint.
- [`docs/porting-notes.md`](../docs/porting-notes.md) for `platform/`.
- [`docs/boards/`](../docs/boards/) for the documentation of each board.
