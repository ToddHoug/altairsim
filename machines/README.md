# machines/

Each `.toml` file in this directory is a built-in machine. A machine file describes a whole
simulated machine: the processor board, the memory, the other boards, and the commands that
run at startup.

The build compiles these files into the program. A user starts one by name and needs no file
on disk:

```sh
altairsim --list        # show the built-in machines
altairsim tarbell       # start the machine from tarbell.toml
```

## How a file gets into the program

1. [`cmake/embed_machines.cmake`](../cmake/embed_machines.cmake) finds every `*.toml` here.
2. The script copies each file, byte for byte, into `machines_generated.cpp`.
3. The program parses that text with the same loader that reads a machine file from disk.

The name of the machine is the file name without `.toml`. The first comment line of the file
is the one-line description that `altairsim --list` prints.

## Add a machine

1. Add `<name>.toml` to this directory.
2. Write the description on the first line, as a comment.
3. Build. The new machine is in `altairsim --list`.
4. Run `./build/altair_tests machines`. That suite loads every built-in machine.

A built-in machine has no folder of its own, so it mounts only built-in media, such as
`builtin:dbl` from [`roms/`](../roms/). A machine that needs a disk or a tape beside it
belongs in [`examples/`](../examples/).

## Read more

- [Machines](../docs/manual/machines.md) and [Configuring a machine](../docs/manual/configuring.md)
  in the *User Manual*, for the format.
- [`docs/config.md`](../docs/config.md) for why the format has this shape.
