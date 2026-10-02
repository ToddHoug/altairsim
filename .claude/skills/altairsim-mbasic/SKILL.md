---
name: altairsim-mbasic
description: Rules for Microsoft MBASIC (BASIC-80 5.21) under CP/M on an altairsim machine — how to type or paste a BASIC program so that no line is lost, when to wait for `Ok`, how to leave the line editor after a `Syntax error`, and how to load, save and stop a program. Use this whenever a task types at the MBASIC `Ok` prompt, enters or runs a `.BAS` program, or a typed BASIC line arrives changed (a lost first digit, two lines joined into one).
---

# MBASIC on altairsim

This skill needs the `altairsim` skill, which tells you how to boot a machine and type at it.
This skill gives only the rules of MBASIC itself. Each rule was replayed on
`examples/cpm/cpm22-buffered.toml`.

MBASIC reads the keyboard in its own way. The simulator delivers each byte one time. The
rules below are the rules of MBASIC.

## Start and stop

```
run {input: "MBASIC\r", until: "Ok"}            # start MBASIC from the A> prompt
run {input: "MBASIC PROG\r", until: "Ok"}       # start it, then load and run PROG.BAS
run {input: "SYSTEM\r", until: "A>"}            # go back to CP/M
```

Ctrl-C stops a program that runs. MBASIC prints `Break in <line>` and then `Ok`.

## The rules

1. **End each line with CR only.** MBASIC reads LF as "continue the same line".
   `10 PRINT "A"` + LF + `20 PRINT "B"` + CR stores one line 10. `RUN` then prints `A 20`
   and `Syntax error in 10`.
2. **After a direct command, wait for `Ok`.** `NEW`, `LIST`, `RUN`, `LOAD` and `SAVE` are
   direct commands. Send each one in its own `run`, with `until: "Ok"`. MBASIC reads the
   first character that arrives before `Ok` and does not use it. `NEW` + CR + `10 PRINT "X"`
   in one input stores line `0 PRINT "X"`.
3. **Program lines can go together.** Lines that start with a number, each ended by CR, can
   be in one input. MBASIC stores each one.
4. **After `Syntax error in <line>`, leave the line editor.** MBASIC prints `Ok`, then the
   line number, and waits in its line editor. Text that you send now is read as editor
   commands, and a command such as `NEW` is lost. Send `Q` with no CR. MBASIC prints `Ok`,
   and the line is unchanged.

## A whole program from a host file

The host file must have CR/LF line ends. The `altairsim-cpm-text` skill tells you how to
check that. A file with LF line ends becomes one long program line, and `LIST` shows
nothing useful.

**Through the keyboard.** `PASTE` is a monitor command. It sends a file to the keyboard of
the guest, and no character is lost.

1. Get the `Ok` prompt.
2. Queue the file: `monitor {command: 'PASTE "PROG.BAS"'}`.
3. Let MBASIC read it: `run {timeout_ms: 60000}`. The call stops with `idle` when the file
   is read.
4. Check the result: `run {input: "LIST\r", until: "Ok"}`.

**Through the disk.** Copy the file with `R` (see the `altairsim-hostbridge` skill), then
load it:

```
run {input: "R PROG.BAS\r", until: "\nA>"}
run {input: "MBASIC\r", until: "Ok"}
run {input: "LOAD \"PROG.BAS\"\r", until: "Ok"}
```

## A program back to the host

`SAVE "PROG.BAS",A` writes the program as text. Without `,A`, MBASIC writes its own binary
format, which only MBASIC can read. Then, at the `A>` prompt, `W PROG.BAS PROG.BAS T` copies
the text to the host.
