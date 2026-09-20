---
name: altairsim
description: Drive the altairsim MITS Altair 8800 / S-100 simulator over its built-in MCP server — boot a machine, type at a guest's console and read what it prints, assemble and run a CP/M program, single-step and debug one, or talk to a real serial device. Use this whenever a task involves altairsim, an Altair 8800, CP/M 2.2, MBASIC, 8080/Z80 assembly on a period machine, or a `.toml` machine file — and whenever you would otherwise reach for expect or a pty to poke a running guest.
---

# Driving altairsim

`altairsim <machine> --mcp` speaks line-delimited JSON-RPC 2.0 on stdio. If it is registered
as an MCP server, its tools are already in your list; otherwise start it yourself and send
`initialize`, `notifications/initialized`, then `tools/call`.

**`--mcp` does not run the machine file's `startup`.** The machine is loaded — disks mounted,
boards fitted — but parked, so nothing blocks before you have control. You boot it.

## The loop

One `run` per guest command, matching the prompt each time:

```
run {from: 0xFF00, until: "A>"}                 # boot CP/M via the DBL PROM at FF00
run {input: "DIR\r", until: "A>"}               # a command, and what it printed back
run {input: "ASM FOO\r", until: "A>", timeout_ms: 120000}
```

- `run` never blocks. It returns on `until`, on the guest going idle at a prompt, on
  `timeout_ms`, `max_steps`, HLT or a breakpoint — read `stopped` to learn which.
- **`timeout_ms` is a ceiling, not a wait.** A 50-second assembly under `timeout_ms: 120000`
  returns in 50 seconds. Set the worst case you will sit through and let `until` end the call.
- **Never pick an `until` that recurs.** A disk that auto-runs `PROFILE.SUB` reprints `A>`
  several times, and input sent while a SUB is running is swallowed. Match something unique
  to the state you want.
- **Control bytes are JSON escapes:** `\u0003` for ^C, `\u001a` for ^Z, `\u001b` for ESC.
  `\x03` is not JSON — it reaches the guest as the three characters `x03`, which is why a
  control byte can seem to vanish while printable text gets through.

`send {text}` types without running, `recv {}` drains output without running, `regs {}` reads
the CPU. `monitor {command}` runs any one monitor command (`MOUNT`, `SET`, `DISASM`, `IN`,
`OUT`, `CONNECT`, …) and returns its text — the escape hatch for anything without a tool.

## Do not hand-roll a pty

Driving a guest with `expect` or a bare pty fights console pacing and recurring prompts. Use
`--mcp`. That is what it is for.

## Ask, don't guess

- `tools/list` is authoritative for the tool surface; `board_types` reports what a board can
  be told, straight off the board itself.
- `monitor {command: "HELP"}` lists every monitor command; `HELP <cmd>` gives one in detail.
- `cheatsheet.md`, beside this skill, is the whole command surface as plain text — generated
  from the program, so it matches the binary you have.

## The full briefing

`references/driving-with-ai.md` is the long form and the place to look before anything past a
boot-and-type: registering the server with a client, the host bridge (`R`/`W`/`HDIR`) and a
complete assemble-extract-fix round trip, building a machine from a bare disk image, the
debugger commands worth reaching for, attaching a real serial port, and the gotchas that cost
an hour each — CR/LF source files, a BIOS that trashes registers, card base versus channel
register. Read it when the task goes past typing at a prompt.

(In a clone of the repository rather than a release package, that file is
`docs/DRIVING-WITH-AI.md` and the cheatsheet is `docs/manual/ref/cheatsheet.md`.)
