# tapes/

This directory holds period software that was distributed on paper tape or cassette, with the
loaders that read it into the machine. It is a working collection for someone who has the
repository. It is not part of the release package.

Some acceptance tests read files from this directory. If you move or rename a file here,
search [`tests/acceptance/`](../tests/acceptance/) for its path first.

## What is here

| Folder | What it holds |
|---|---|
| `4KBasic31/` | The loader for Altair 4K BASIC 3.1, as source and listing. |
| `8KBasic32/` | Altair 8K BASIC 3.2 as a tape image, its loader, and a machine file that loads it. The 8K BASIC acceptance test loads this tape. |
| `MSBasic10/` | The loader for the first Altair BASIC, version 1.0, as source and listing. |
| `MitsPS2/` | MITS Programming System II: the monitor tape, the editor, the assembler and the debugger, with two machine files. The Programming System II acceptance tests load these tapes. |
| `Basic Versions.pdf` | A table of the Altair BASIC versions. |

## The file types

| File | What it is |
|---|---|
| `.tap`, `.TAP`, `.BIN` | A tape image: the bytes on the tape, in order. |
| `.ASM`, `.PRN`, `.HEX` | A loader, as source, assembler listing and Intel HEX. |
| `.toml` | A machine file. Its paths start from its own folder, so it finds the tape beside it. |
| `README.md`, `-ReadMe.pdf` | Our notes, and the notes that came with the software. |

## Where other tapes are

| Location | What it holds |
|---|---|
| [`tests/media/`](../tests/media/) | The other tapes that the acceptance tests load. The media for a new test goes there. |
| [`examples/`](../examples/) | The examples that ship in the release package, each with its machine file and its media. |

## Read more

- [Tapes](../docs/manual/tapes.md) in the *User Manual*, for the cassette interface and how to
  load BASIC.
- [`docs/sources.md`](../docs/sources.md) for where each tape came from.
- [`tools/tapetool.cpp`](../tools/tapetool.cpp), a tool that reads and writes cassette audio
  with no simulator attached.
