# Running it

`altairsim` is one program file. Unzip the package, and run the program from a terminal.

```
$ ./altairsim
AltairSim X.Y.Z -- 8080, full speed.
machine: default.  HELP for commands.
altairsim>
```

On **Windows**, the program is `altairsim.exe`:

```
> altairsim.exe
```

You can run the program from any folder. Put its folder on your `PATH`, and type `altairsim`.

`X.Y.Z` is the version of your release. A program built between two releases adds the commit
that it was built from, for example `AltairSim X.Y.Z-37-gcc64cca`.

The `altairsim>` prompt is **the monitor**. The machine has memory, a processor, a console board
and a floppy disk controller in it. The machine has power, but it is stopped. No program runs
until you start one.

## macOS: the first run

macOS marks a file that comes from the internet. It does not run a marked program that is not
signed. If you see *"cannot be opened because the developer cannot be verified"*, remove the
mark:

```
$ xattr -dr com.apple.quarantine ./altairsim
```

You do this one time. macOS does this to every unsigned program in a downloaded archive.

A browser or a mail client adds the mark. If you got the archive with `curl` or `scp`, the file
may have no mark. In that case, the command does nothing, and that is correct.

## Getting help, and getting out

| Type | To |
|---|---|
| `HELP` | list every command |
| `HELP DUMP` | show the usage and examples for one command |
| `QUIT` | leave the program |

**There is no `EXIT` command.** Type `QUIT`, or only `Q`.

You can type a short form of a command. `HELP` shows each command with brackets, for example
`D[UMP]`, `DE[POSIT]` and `RES[ET]`. The part before the bracket is the short form. You can type
the short form, the full name, or any length between them.

**Commands are not case-sensitive**, and the names of the boards in the machine are not
case-sensitive. This manual writes commands in capitals because they are easier to read.

## Which machine you get

When you run `altairsim` with no arguments, you get the machine called `default`. It is a 56K
Altair with a console, a floppy disk controller and a boot PROM.

To get a different machine, give its name or its file:

```
$ altairsim examples/cpm/cpm22-buffered.toml     a machine file: this one boots CP/M
$ altairsim basic4k                     a BUILT-IN machine, by name
$ altairsim --list                      what the built-in names are
```

A **built-in** machine is a machine file that is stored in the program. It uses the same format
as the machine files in `examples/`. To see what is in a built-in machine, load it and look:

```
$ altairsim basic4k -x BOARDS
```

To get a built-in machine as a file that you can edit, type `CONFIG SAVE mine.toml`. This
command writes the machine that you are running to a file, and that file boots.

The machines chapter describes every command-line option. It also tells you how `altairsim`
decides if a word is the name of a built-in machine or the name of a file.
