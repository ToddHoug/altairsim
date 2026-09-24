# The MCP server

```
$ altairsim --mcp
```

This command runs `altairsim` as an **MCP (Model Context Protocol) server** on stdin and stdout.
An AI assistant, such as Claude or any other program that uses MCP, can then control the machine
through **typed, structured tools**. It does not have to read text from a terminal screen.

**The MCP server uses the same machine as the monitor.** It is not a separate model of the
machine. What the tools see is what `SHOW` sees, because it is the same machine that answers the
same questions in a different way.

## What the tools do

The tools let an assistant operate the whole machine:

- List the available board types, and every property of each type, **with its type, its default
  and its legal range.**
- List the boards in the machine.
- Get and set any property of any board.
- Add a board, mount a disk or a tape in it, and connect a serial line to an endpoint.
- Examine, deposit, fill, search, save and disassemble memory.
- Run the machine, step it, set breakpoints, and read the bus trace.
- Save the state of the whole machine in a snapshot, and restore it.

Five of the tools are `board_types`, `board_list`, `board_get`, `board_set` and `board_add`. The
other tools use the same words as the monitor.

## Controlling a running guest

To build a machine is one half of the work. The other half is to **operate a running machine**:
to type at its console and read what it prints. These tools do that. With them, an assistant can
boot CP/M, run `ASM`, and talk to a program over a serial port, all through MCP:

- **`run`** runs the guest for a limited time, and returns what it printed. Pass `input` to type
  a line, and `until` to stop when a string appears, such as the prompt `A>`. Pass `from` to set
  the PC first. To boot, give the address of the boot PROM. `run` **also stops by itself when
  the guest reaches a prompt**, which means that the guest waits for console input with nothing
  to print. You get control back without having to guess a timeout. Every stop gives its reason
  in `stopped`: `match`, `idle`, `timeout`, `steps`, `halt`, `breakpoint` or `interrupted`.
- **`send`** types at the console without running. Use `run` after it, so that the guest reads
  the input.
- **`recv`** gets what the guest printed since you last looked, without running.
- **`regs`** gives the processor registers now.
- **`status`** is a check that never blocks. It tells you whether the server is busy on ANY
  call, not only on `run`. It also gives the processor board id, and the step count and PC of
  the last `run`. It never waits behind another call, even behind a `run` that does not end. See
  "Stopping a `run` that does not end", below.

A session looks like this. Boot with `run {from: 65280, until: "A>"}`, then send one
`run {input: "ASM FOO\r", until: "A>"}` for each command, and read the reply each time. In JSON,
`from` is a decimal number: `65280` is `FF00` hex. JSON has no `0x` form.

A `run` **never blocks.** It runs the guest for at most `timeout_ms` (default 2000, maximum
600000), and returns. For this reason, a `tools/call` always comes back. A plain `RUN` through
the `monitor` tool is different. Under a pipe, it waits on stdin, and stdin is the JSON-RPC
channel itself.

`timeout_ms` is a limit, not a wait. The call ends as soon as `until` matches or the guest
reaches a prompt. More time than the work needs costs nothing. A job that takes fifty seconds,
with a limit of two minutes, returns after fifty seconds. Set the limit to the longest time that
you are willing to wait, not to the time that you expect, and let `until` end the call.

What you type goes to the guest byte for byte, with control characters, and every line in the
machine is 8-bit clean. Write a control byte as a JSON `\uXXXX` escape: `\u0003` is `Ctrl-C`,
and `\u001a` is `Ctrl-Z`. For example, `send {text: "\u0003"}` stops a running BASIC program,
and `run {input: "\u001a"}` ends a `PIP` copy from the console, as the keys would. `\x03` is
**not** JSON, because JSON has no `\x` escape. It arrives at the guest as the three ordinary
characters `x03`, not as a control byte.

By default, the guest runs as fast as possible, which is correct for booting and for using a
prompt. When a real device is on a serial line and you set a clock speed with
`SET cpu0 clock_hz=…`, `run` paces the guest to that clock. A reply that the device sends a
short time later then arrives while the guest still waits for it.

`timeout_ms` is a fixed limit in real time. Traffic on a live line does not extend it. For
example, a boot loader that reads its whole system over a serial disk is limited in the same way
as any other call. Give it a `timeout_ms` as long as the worst case (up to 600000 ms), and let
it return early on `until` or on a prompt. A call that reaches `timeout_ms` in the middle of a
transfer returns `stopped: "timeout"`, with what it has read so far. This is a normal result,
not a failure. Call `run` again in a loop. `regs` or `mem_dump` can show that a destination
pointer still increases.

### Stopping a `run` that does not end

A `run` ends by itself at `timeout_ms`, but you may not want to wait that long. There are two
ways to stop it early. Both stop the `run` at once. It returns `stopped: "interrupted"`, with
what the guest printed so far.

- **Cancel the request.** Send the standard MCP `notifications/cancelled` message, with the
  request id of the `run`. The server continues to read its input while a `run` goes on, so it
  sees the cancel at once. The server ignores a cancel that names another request, or that
  arrives after the `run` has returned, and a cancel never applies to the next call. Other
  requests that you send during a `run` wait, and the server answers them in order after the
  `run` returns. `status` is the exception, because it never waits. Use it to check whether a
  `run` that you want to cancel is still going, or has already stopped.
- **Send the process a `Ctrl-C`.** Press it in the terminal that started the server, or send
  `kill -INT` to its process ID.

After either one, the machine is as it was when it stopped. You can look at it, and continue
with another `run`. A `Ctrl-C` that arrives when no `run` is going does nothing to the guest,
and a new `run` always starts clean.

This changes what `Ctrl-C` does to an `--mcp` server that you started by hand. The program
catches the first `Ctrl-C`, and it does not end. If you press `Ctrl-C` again before the server
has reported the first one, the second one ends the server, as `Ctrl-C` usually does. A server
started in the background, or with `nohup`, ignores `Ctrl-C`, as any such program does.

The `pc` and `steps` fields of `status` come only from the last `run`. A `step` or a `monitor`
command moves the real PC without changing them. `steps` goes back to zero on the next `run`, so
it does not always increase. `generation` always increases, so use it to tell whether the
machine still runs or is stuck on the same slice.

Under `--mcp`, the program moves the console line onto a terminal in memory that the server
owns, because there is no host keyboard behind a pipe. `send`, `run` and `recv` read and write
this terminal. Everything else in the machine continues to run, and the program services it on
every `run` slice. This includes a second serial board connected to a real port, and a socket. A
program that moves bytes between the console and a modem port works as it would at a real
terminal.

## Watching while an assistant works: `--mirror`

Add `--mirror socket:PORT` with `--mcp`, and a person can type `telnet localhost PORT` to
**watch the session that the assistant controls**, with every character that the guest prints.
That person can also **type on the line to take over**, and share the console:

```
$ altairsim examples/cpm/cpm22-buffered.toml --mcp --mirror socket:2323
```

This puts the assistant's console in the same mirror that the monitor has
(`<endpoint>|socket:PORT`, see the chapter *Serial ports, sockets and telnet*). The assistant
continues to use `run`, `send` and `recv` as before, and the mirror does not change anything for
it. What it types, and what the guest prints, also go over the socket to the watcher. Add `?ro`
to make the mirror watch-only. **Put quotes around it on the command line**, because `?` is a
shell wildcard:

```
$ altairsim examples/cpm/cpm22-buffered.toml --mcp --mirror 'socket:2323?ro'
```

Without the quotes, the shell tries to find a file called `socket:2323?ro`, and fails before
`altairsim` sees it. (zsh reports `no matches found`.) One watcher can connect at a time.

The watcher never sets the speed. For this reason, the guest runs **only during a `run`**. A
character that the watcher types between runs waits on the line, and the guest reads it on the
next `run`, as with `send`. While a `run` goes on, the watcher and the assistant share the
console live.

## Debugging and inspecting

The monitor's debugger is also available here, as structured tools:

- `step` runs a number of instructions, and returns the registers and the place where the
  processor stopped.
- `breakpoints` lists, adds and removes the same breakpoints that `BREAK` sets. They are the
  machine's breakpoints, so a `run` or a `step` stops when one of them is reached.
- `disasm` decodes memory with a read that changes nothing, so it works on a ROM, and also with
  no processor running.
- `bus_trace` returns the bus history, which is always recorded: the last cycles that every
  board saw, which board drove each one, and which board answered.
- `bus_irq` reports the interrupt lines, as `bus_map` reports the address decode.
- `snapshot` and `restore` save the state of the whole machine, and read it back into a machine
  that is built in the same way.

None of these block, and none of them need the console. They ask questions about the machine,
and the answers are the same as from `SHOW`. When a typed tool cannot do what you need, such as
a conditional breakpoint or an octal listing, the `monitor` tool runs any monitor command and
returns its text.

## The schemas describe themselves

**The schema of every tool comes from the same source as the TOML keys and the `SET` and `SHOW`
commands.** The program has one description of what a board is and what you can ask it. The
machine file loader, the monitor and the MCP server all read that description.

For this reason, **an assistant can control a new board as soon as the board is added, with no
new code.** Nobody writes an MCP tool for the new board. The board declares its properties, as
it must to be configurable at all, and the tool schema is that declaration.

For this reason, this manual has no tool reference. Start the server and ask it what it has.
`tools/list` returns every tool of the server, and `board_types` returns every board type. Their
answers are always correct for your copy of the program, and a printed list cannot be.

## Setting up an assistant to use it

MCP clients are different, but they all need the same two things: a command to run, and the fact
that the server uses stdio. The command is `altairsim <machine> --mcp`, and it uses stdio.
Register it **one time**, and after that you talk to the assistant, not to the server.

**Claude Code (the command line)** takes it as one command. Everything after `--` is what it
runs. Run it from the folder that you want the machine's files to resolve against:

```
claude mcp add altairsim -- altairsim <machine> --mcp
claude mcp list                       # confirm it registered and is reachable
```

**Claude Desktop, or any client that reads a JSON configuration**, needs an `mcpServers` entry
with the command and its arguments. A `cwd` sets the working folder, because a desktop
application has no shell to get one from:

```json
{
  "mcpServers": {
    "altairsim": { "command": "altairsim", "args": ["<machine>", "--mcp"] }
  }
}
```

`<machine>` is a built-in name or a machine file, as on the command line. If `altairsim` is not
on your `PATH`, give its full path as the command.

You can register more than one. Register several machines under different names (`altair-cpm`,
`altair-basic`), and an assistant sees all of them. The *scope* of `claude mcp add` decides
where a server is available:

- the default: only in the project folder that you added it from
- `--scope project`: in a `.mcp.json` file that moves with the folder
- `--scope user`: everywhere

For file transfer, know this: the host-bridge sandbox is the server's working folder. The
`claude` command line sets it to the folder that you started from, so a relative machine path
and the sandbox both point at your folder. The desktop application currently starts the server
in your home folder, so there, give the machine a full path.

`DRIVING-WITH-AI.md`, in this package, is the briefing for the assistant itself. Put it in a
working folder, and the assistant has the steps for booting, building and debugging with these
tools. The `examples/ai-mcp/` folder is a working folder of this kind, with a walkthrough in
which an assistant finds and fixes a bug, only through MCP.

## Starting a project of your own

The `examples/ai-mcp/` folder is a guided example. When you want to write your own software,
start in the same way. **Make a folder of your own, put a machine in it, and point the assistant
at that folder, not at the copy in the package.**

**Put the machine and its images at the top of the folder.** Copy a machine that boots the
system that you want to build on into a new folder, and work there. The whole `examples/ai-mcp/`
folder is a good start, with its disk and machine file. Register the server from inside that
folder, as in the section above, so that the machine file and the host-bridge sandbox both
resolve there. The guest writes to the disk image as it runs, and the assistant leaves build
files beside it. If you keep the copy in the package unchanged, you always have a clean one to
start again from. Also keep a spare copy of the machine and its new disk, so that "start again"
gives you a known state, not what the guest last wrote.

**Give it local information to read.** Put `DRIVING-WITH-AI.md` in the folder. Keep a second
folder, for example `Reference`, for anything else that you want the assistant to use: the parts
of this manual that are important to your project, and your own source material, converted to
plain Markdown. An assistant reads Markdown most reliably. These copies go out of date when
`altairsim` changes, and the assistant cannot tell an old copy from a current one. Update them
when you update `altairsim`.

**Watch what it does, because it uses what it already knows.** An assistant does not read the
files in your folder and then do what they say. It chooses its next step from everything in
front of it, and from everything that it learned in training. What it learned is very large, and
one line in a local file has little weight against it. For this reason, when the job looks like
a common one, such as booting CP/M, assembling a file or copying something to a disk, the
assistant first tries the way that it has seen most often. It does this even when the files in
your folder say something else. For example, when you ask it to get a program onto a disk, it
tries a host tool such as `cpmtools`. That tool does not understand the hard-sectored disks of
the Altair. The way that works is to build inside the machine over the host bridge, as
`DRIVING-WITH-AI.md` says. The longer a session runs, the more the assistant goes back to the
common way, and when it starts down the wrong way it tends to continue.

For this reason:

- Watch what it sends and reads back. The `--mirror` socket above is for this. Do not trust its
  own summary of what it did.
- Correct it at the first wrong step, before it builds on that step.
- Point it at the file that you mean, and tell it to use that file and only that file.
- Be most careful when its answer looks the most usual. That is when a habit has most probably
  taken the place of something that is special about your machine.

**Start and stop each session in the same way.** A short instruction that you give every session
saves time. At the end, tell it to clean up: close any processes that still run, write down what
it learned, and save where you are in a note in the folder. At the start, tell it to read that
note and the `Reference` folder, boot the machine, and tell you where you stopped. The note in
the folder lets a session continue. Without it, the assistant must work out the state again each
time, and it gets it wrong.

**Do not rename the project folder in the middle of a project.** Claude Code keeps its memory of
the project under the full path of the folder, outside the folder itself. After a rename, the
folder looks like a new project to it. The registration from the section above is also tied to
that path by default. `claude mcp list` in the renamed folder no longer shows `altairsim`, and
the assistant cannot reach the machine until you run `claude mcp add` again. `--scope project`
prevents this, because the registration is then in a `.mcp.json` file that moves with the
folder.

If it happens, register the server again, tell the assistant the old and the new folder names,
and tell it to start from the note in the folder, the note that "Start and stop each session in
the same way", above, tells you to keep.
