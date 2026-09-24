# What altairsim is

`altairsim` simulates the **MITS Altair 8800** and the **S-100 bus** that the Altair uses.

It boots real software. This is the software as it was sold, byte for byte. It was not written
to work with `altairsim`. Examples are Altair BASIC from a cassette, CP/M 2.2 from an 8″
floppy disk, and MITS Programming System II from paper tape. None of this software is patched.
None of it can tell that it runs on a simulated machine.

You can use the machine as a test bench. You can run a driver that you are writing against a
board that is in the program, before you have that board as hardware.

Bugs are also easier to find. You can stop the machine at a bus cycle that you choose. You can
see which board answered and which board did not. When you give the machine the same input
again, you get the same result.

## What it does

- **Three processors.** The 8080, the 8085 and the Z80 are each here. The 8085 runs all 8080
  code. Each processor gives the correct flags and carries, and its undocumented behavior. Each
  one is tested with the standard processor exerciser programs.
- **Period boards.** There are processor boards, memory boards, serial boards, cassette
  interfaces, disk controllers, a line-printer controller and the front panel. The boards
  chapter lists them all.
- **Video.** The Altair had no video, but video boards were made for the S-100 bus. Several of
  them are here: the VDM-1, the Dazzler, the SD Systems VDB-8024 and the Sol-PC. Each video
  board opens a window on your screen.
- **A monitor.** The monitor is the `altairsim>` prompt. You get it when the machine is
  stopped. From the monitor you can:
  - set breakpoints, with or without a condition
  - step one instruction at a time
  - disassemble memory
  - examine and change memory
  - trace the bus cycles, and look at the instruction history and the bus history
  - look at the bus itself: which board decodes which address, which board pulls which
    interrupt line, and where two boards respond to the same address

  *The Monitor* and *The Debugger* are two documents that ship beside this manual. They
  describe these commands.
- **Real input and output.** You can connect a serial board to your terminal, to a TCP socket,
  or to a real serial port on your computer. With a TCP socket, you can use telnet to connect
  to the guest. With a real serial port, the modem control lines are connected too. You can
  connect a line-printer board to a **real print queue**. A listing from 1977 then prints on
  the printer on your desk.
- **File transfer** between your computer and CP/M. A board of our own, which is not a period
  board, moves the files. The guest can use only the one folder that you choose. The commands
  you type are `HDIR`, `R` and `W`. These are ordinary CP/M programs, the same as `PIP` or
  `STAT`. You run them at the `A>` prompt.

## What it does not do

This section tells you the limits before you start.

- **You can save the state of the machine, but you cannot replay a session.** `SNAPSHOT` writes
  the whole state of the machine to a file. `RESTORE` reads the file back. You cannot record a
  session and then step backward through it.
- **There is no audio output.** The program reads and writes cassette `.WAV` files as files.
  The tapes chapter tells you how.
- **Not every S-100 board is here.** The boards chapter lists the boards that are.
- **The timing is correct for the software, but it is not a circuit simulation.** Each
  instruction takes the correct number of T-states. A cassette takes the correct number of
  T-states to load. The program does not model propagation delay or analog behavior. No
  software from the period can see the difference.

## What is in the box

You have the `altairsim` program, this manual, and a folder of worked examples with their
media. Each example is a complete machine that boots, and each has its own README. More
machines are built into the program. The program can boot a machine as soon as you unzip it.
The next chapter tells you which machine, and where to get more disks and tapes.

There is no installer, and there is nothing to set up. Unzip the package and run the program.
