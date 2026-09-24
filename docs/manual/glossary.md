# Glossary

**ACIA**: Asynchronous Communications Interface Adapter. The Motorola 6850 chip on the 88-2SIO
serial board. A program can see two of its registers: a status register and a data register. A
program does everything with a serial port on this machine through one of those two registers.

**ACR**: Audio Cassette Recorder interface. The MITS 88-ACR, a board that wrote bits to an
ordinary audio cassette and read them back. People who could not pay for a floppy disk loaded
BASIC with it, and in 1976 that was most people.

**ATTN**: See **STOP**. ATTN is the old name of the STOP key. `CONSOLE attn=` still works, and
it means the same as `CONSOLE stop=`.

**backplane**: The circuit board with the connectors that every board plugs into. It is not one
of the boards. A machine file adds no backplane, because a machine file describes what is in the
backplane. On an Altair, the backplane is the S-100 bus: eighteen slots of one hundred pins
each, all connected in parallel. There is no chipset. The backplane *is* the machine.

**bank switching**: A way to give the processor more memory than it can address. Several banks
of memory use the same addresses, and a switch selects one bank at a time. An 8080 can address
64K and no more, so a machine with 128K of RAM shows the processor one part of it at a time.

**BDOS**: Basic Disk Operating System. The middle layer of CP/M, which handles files,
directories and the console. A CP/M program asks the BDOS for what it needs, and does not know
which hardware answers.

**BIOS**: Basic Input/Output System. The bottom layer of CP/M, and the only part that knows what
machine it is on. To move CP/M to a new computer, you rewrite the BIOS. The BDOS and the CCP do
not change. A track buffer is also in the BIOS, *if the author put one there*. For this reason,
on some versions of CP/M and not on others, a file can be "written" and not yet be on the disk.
See the disks chapter.

**board**: Anything that plugs into the backplane: memory, a serial port, a disk controller, the
front panel, or the processor itself. This program uses the word everywhere. `BOARDS` lists
them, and `[[board]]` adds one in a machine file. A board is the thing that you add, remove,
`SHOW` and `SET`. See also **card**, which is the same object.

**card**: The same object as a **board**. The period hardware and its manuals said "card". This
manual uses the word only when the sentence is about the physical object that somebody bought,
put chips into and set jumpers on. Everywhere else, it says board.

**CCP**: Console Command Processor. The top layer of CP/M, which prints `A>` and runs what you
type. A large program can write over it, and CP/M loads it again from the disk afterward. That
load is the warm boot.

**contention**: Two boards that answer the same address. On a real S-100 machine, both would
drive the data bus at the same time, and you would get a byte that is neither board's value,
sometimes and not always. That is very hard to find. `SHOW BUS CONTENTION` names the boards.

**CP/M**: Control Program for Microcomputers. The disk operating system of Digital Research, and
the reason that the 8080 was important. A program written for CP/M ran on 8080 machines from
many different makers.

**CUTS**: Computer Users Tape System. The cassette format of the Sol-20, and a faster version of
Kansas City. At its default of 1200 baud, it uses tones one octave lower, **1200/600 Hz**, and
puts one bit in one cycle (a "1") or in a half cycle (a "0"). Its slower 300-baud mode is Kansas
City, 2400/1200 Hz. The Sol's UART does both, and the guest selects one with bit D5 of
`OUT 0FAh`.

**DBL**: Disk Boot Loader. The boot PROM of the MITS floppy disk controller, at `FF00`. It loads
the boot loader from the disk, and the boot loader loads CP/M. **There is no `BOOT` command on
an Altair.** You set the address switches to `FF00`, press EXAMINE to load them into the program
counter, and then press RUN. DBL is the program that then runs.

**decode**: What a board does when it recognizes an address as its own, and answers. A board
that does not decode an address stays silent, and another board can answer. Which board decodes
which address is the main question of how an S-100 machine is put together.

**DMA**: Direct Memory Access. A board takes the bus from the processor and reads or writes
memory itself, without the processor. It is faster, and it can cause faults that are difficult
to find.

**endpoint**: In this program, the thing at the far end of a serial unit's cable, for example
`console`, `null`, a TCP socket or a real serial port. The serial chapter has the complete list.

**floating bus**: What the data bus reads when nothing drives it. On the S-100 bus it floats
high, so an `IN` from a port that no board decodes returns `FF`. This is not an error. It means
that no board is there.

**front panel**: The switches and lamps on the front of the Altair: the address switches, the
data lamps, and DEPOSIT, EXAMINE, RUN, STOP and RESET. Before there was a terminal, the front
panel *was* the user interface, and you toggled in your bootstrap through it, one byte at a
time. In this program, it is a board like any other.

**FSK**: Frequency Shift Keying. A way to encode bits as two audible tones, one for a zero and
one for a one. The ACR used it to put data on a cassette, and it is the reason for the sound of
a tape that loads.

The tones were **not** the same on all machines, and this is important. The 88-ACR uses
2400/1850 Hz, and holds a tone for the whole bit. Kansas City counts *whole cycles* of 2400/1200
Hz for each bit. CUTS, at its default of 1200 baud, does the same one octave lower (1200/600 Hz,
with a half cycle for a "0"). A board can read only the modulation that its own modem was built
for. For this reason, the program refuses a recording in the wrong modulation, and does not
decode it. See the tapes chapter.

**guest**: The software that runs on the simulated machine, such as CP/M, BASIC or a program
that you wrote. The guest cannot tell that the machine is simulated.

**hard-sector**: A floppy disk where physical holes in the disk mark the sector boundaries, and
the controller counts them as they go past. The MITS floppy disks are hard-sectored. A
soft-sectored disk has one hole, and finds its sectors from marks written in the data. Soft
sectors became the common method.

**IntAck**: Interrupt Acknowledge. The bus cycle that the 8080 runs when it accepts an
interrupt. The board that interrupts puts one instruction on the data bus during that cycle, and
the processor executes it. It is almost always an `RST`.

**Kansas City standard**: The 1975 agreement on how microcomputers should record data on
ordinary audio cassettes, named after the meeting where it was agreed. A one is eight cycles of
2400 Hz, and a zero is four cycles of 1200 Hz. Both take the same time. For this reason, a
receiver counts cycles and does not depend on a clock, so a tape that plays 5% slow still reads.
It runs at 300 baud. See also **CUTS**, its faster version, and **FSK**.

**machine file**: The `.toml` file that describes a machine: its boards, their settings, and the
commands to run when it starts. The configuring chapter describes the format.

**MCP**: Model Context Protocol. The protocol that an AI assistant uses to call structured
tools. `altairsim --mcp` uses it, so that an assistant can control the machine directly. See the
MCP chapter.

**monitor**: The `altairsim>` prompt. It is not a program that runs in the machine. It is you,
in front of the machine, with more controls than the real front panel had.

**PHANTOM\***: An S-100 line that tells memory boards to stop answering. When a board pulls it,
a ROM can answer at an address that a RAM board also decodes, with no contention. In this way, a
boot PROM can cover RAM and then get out of the way. The asterisk means that the line is active
low, as most lines on this bus are.

**pINT**: The S-100 interrupt request line. Any board can pull it. The processor sees it, if
interrupts are enabled, and runs an IntAck cycle to find out which board interrupted and why.

**PROM**: Programmable Read-Only Memory. A chip with a program in it that stays when the power
is off. The Altair's boot loader is in one. Without it, a disk machine could not start, because
something must already be in memory to read the disk.

**RST**: Restart. A one-byte 8080 instruction that calls a fixed low address: `RST 0` to
`RST 7`, at `0000`, `0008` and so on to `0038`. It is one byte, so it fits in an IntAck cycle.
That is what it is for.

**S-100**: The bus. One hundred pins and eighteen slots. MITS designed it for the Altair, and
then other makers used it. It was an early open standard in personal computing, and for this
reason, a board from one company worked in the machine of another company.

**sector**: The smallest part of a disk that you can read or write. On these floppy disks, a
sector is 137 bytes, and 128 of them are yours.

**SENSE switches**: The top eight address switches (A8 to A15) on the front panel. A program can
read them as an input port. Software used them for settings before there was any other place for
them: which port is the console, how much memory to use, or whether to load from tape.

**STOP**: The STOP switch on the front panel, and the key that presses it, `Ctrl-E` by default.
It stops the processor and gives you the monitor. It does not change the machine. It is not
RESET, and it is not POWER. For this reason, `RUN` with no address continues at the instruction
that the machine was about to execute. The program reads the key before the guest sees the byte,
so no guest program can take it from you. Move it with `CONSOLE stop=`. The older `attn=` still
works.

**T-state**: One clock cycle. Every 8080 instruction takes a known number of them, and this
program uses them to measure time. At 2 MHz, a T-state is 500 nanoseconds.

**TDRE**: Transmit Data Register Empty. The bit in the 6850's status register that means "the
board has room for another character". A program that prints reads this bit until it is set. A
serial port connected to `null` has it set all the time. For this reason, a program that writes
to nothing works.

**track**: One ring of sectors on a disk. The head steps in and out to reach a track, and does
not move again to reach the sectors on it. For this reason, a BIOS with a buffer keeps a whole
track at a time: after one seek, it can read or write every sector of the track.

**UART**: Universal Asynchronous Receiver/Transmitter. The chip that changes a byte into a
series of bits on a wire, and back again. The ACIA is one.

**unit**: One channel of a board that moves characters, like the connector on the back of the
board. A 2SIO has two units, `a` and `b`, and you connect each one separately. `CONNECT sio0:b`
names the board and then the unit.

**VI0–VI7**: The eight vectored interrupt lines of the S-100 bus, served by the 88-VI/RTC board.
**VI0 has the highest priority.** A board that pulls VI*n* gets `RST` *n*, and the interrupt
board decides which board wins when two pull at the same time.

**warm boot**: CP/M loads its own top layer (the CCP) again from the disk, because the program
that had finished could write over it. `Ctrl-C` at the `A>` prompt does a warm boot on purpose.
It is not a reset, and the machine does not stop.
