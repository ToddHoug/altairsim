# What altairsim is

`altairsim` simulates the **MITS Altair 8800** and the **S-100 bus** that the Altair uses. A
machine is a set of boards that plug into the bus. The program has built-in machines, and you
can describe your own machine in a machine file.

The machine runs the software of the period as it was sold. Examples are Altair BASIC from a
cassette, CP/M 2.2 from an 8″ floppy disk, and MITS Programming System II from paper tape. The
software is not patched, and it cannot tell that the machine is simulated.

## What you can do with it

- **Run period software.** Boot CP/M, load BASIC from a tape, and run the programs of the
  1970s.
- **Write software for the machine, and find its bugs.** You can stop the machine at an
  instruction or a bus cycle that you choose. You can see which board answered and which board
  did not. When you give the machine the same input again, you get the same result.
- **Test a driver before you have the hardware.** Write a driver for a board that the program
  has, and run it against that board.
- **Connect the machine to your computer.** A serial board can use your terminal, a TCP socket
  or a real serial port. A line-printer board can print on the printer on your desk.
- **Let an AI assistant use the machine.** An assistant can boot the machine, type at it and
  debug a program through the MCP server.

## What is in the program

- **Three processors:** the 8080, the 8085 and the Z80. The 8085 runs all 8080 code. Each
  processor gives the correct flags, including its undocumented behavior. Each one is tested
  with the standard processor exerciser programs.
- **Period boards:** processor boards, memory boards, serial boards, cassette interfaces, disk
  controllers, a line-printer controller and the front panel. The boards chapter describes each
  one.
- **Video boards.** The Altair had no video, but video boards were made for the S-100 bus. The
  VDM-1, the Dazzler, the SD Systems VDB-8024 and the Sol-PC are here. Each video board opens a
  window on your screen.
- **The monitor.** The `altairsim>` prompt controls the machine while it is stopped. From the
  monitor, you can set breakpoints, step one instruction at a time, disassemble, change memory,
  and trace the bus. You can also see which board decodes each address, and which boards answer
  the same address. *The Monitor* and *The Debugger* are two documents that ship beside this
  manual, and they describe these commands.
- **File transfer** between your computer and CP/M. A board that the period did not have moves
  the files, and the guest can use only the one folder that you choose. At the `A>` prompt, you
  type `HDIR`, `R` and `W`. These are ordinary CP/M programs, like `PIP` or `STAT`.

## What it does not do

- **You can save the state of the machine, but you cannot replay a session.** `SNAPSHOT` writes
  the whole state of the machine to a file, and `RESTORE` reads it back. You cannot record a
  session and step backward through it.
- **There is no audio output.** The program reads and writes cassette `.WAV` files as files.
  The tapes chapter tells you how.
- **Not every S-100 board is here.** The boards chapter lists the boards that are.
- **The timing is correct for the software, but it is not a circuit simulation.** Each
  instruction takes the correct number of T-states. The program does not model propagation
  delay or analog behavior. No software from the period can see the difference.

## Where to start

There is no installer, and there is nothing to set up. Unzip the package and run the program.

- To boot CP/M now, go to the *Quick start*. It takes one command.
- To learn what came in the package, read the next chapter.
- If the program does not start, read *Running it*.
