# docs/

This directory holds the documentation: the documents that ship in the release package, the
documents for someone who changes the source, and the files that build the PDFs.

## Documents that ship in the package

A reader of these documents has the package and does not have the repository.

| Source | PDF | What it is |
|---|---|---|
| [`manual/`](manual/README.md) | `altairsim-manual.pdf` | The *User Manual*. `manual/ref/` holds its reference chapters. |
| [`monitor/`](monitor/) | `altairsim-monitor.pdf` | The commands at the `altairsim>` prompt. `monitor/ref/` holds its command reference. |
| [`debugger/`](debugger/) | `altairsim-debugger.pdf` | How to debug a guest from the monitor. |
| [`changelog/`](changelog/) | `altairsim-changelog.pdf` | What changed in each release. |
| [`QUICK-START.md`](QUICK-START.md) | `QUICK-START.pdf` | How to boot CP/M with one command. |
| [`migrating.md`](migrating.md) | `migrating.pdf` | A guide for a user of AltairZ80 (SIMH) or z80pack. |
| [`DRIVING-WITH-AI.md`](DRIVING-WITH-AI.md) | none | How an AI assistant drives a machine through the MCP server. It ships as Markdown. |
| `manual/ref/cheatsheet.md` | `altairsim-cheatsheet.pdf` | The quick reference on one page. |

A document that is a folder has an `ORDER` file. `ORDER` lists the chapters in sequence, and a
chapter that is not in `ORDER` is not in the PDF.

## Documents for someone with the source

| Source | What it is |
|---|---|
| [`devguide/`](devguide/README.md) | The Developer Guide: how the machine works inside and how to add a board. It also builds to `altairsim-devguide.pdf`. |
| [`boards/`](boards/) | One file for each board: the real hardware, the registers, the quirks, and what is not modelled. `_TEMPLATE.md` is the form for a new board. |
| [`config.md`](config.md) | Why the machine file format has its shape. |
| [`cli-commands.md`](cli-commands.md) | Why the monitor commands rank and abbreviate as they do. |
| [`roms.md`](roms.md) | The built-in ROMs. |
| [`sources.md`](sources.md) | Where each hardware fact, ROM, tape and disk came from. |
| [`porting-notes.md`](porting-notes.md) | Where the three operating systems differ, and lessons from earlier work. |
| [`printing.md`](printing.md) | The design of the `printer:` endpoint. |
| [`building-linux.md`](building-linux.md), [`building-windows.md`](building-windows.md) | How to build on each platform. |

## Files that build the PDFs

| File | What it is |
|---|---|
| [`package.map`](package.map) | The list of what the release package holds, and the path tokens that the manual uses. |
| `print.css` | The page layout of the PDFs. |
| [`fonts/`](fonts/README.md) | The fonts that the PDFs embed, with their licences. |
| [`pagedjs/`](pagedjs/README.md) | The library that gives the PDFs page numbers and a table of contents. |

[`tools/build-docs.sh`](../tools/build-docs.sh) builds the PDFs. It is separate from the
program build.

## Rules

- **Do not edit `manual/ref/` or `monitor/ref/` by hand.** The program generates
  those files. Change the emitter, [`tools/gen-reference.cpp`](../tools/gen-reference.cpp),
  then run `cmake --build build --target docs-reference`.
- **Do not commit a PDF that you built.** CI builds the PDFs and commits them. If you ran
  `tools/build-docs.sh`, restore the PDFs with `git checkout --` before you commit.
- **The *User Manual* names only what is in the package.** It does not name a source file, a
  test or `DESIGN.md`. A test fails the build if a chapter does. The Developer Guide has no
  such limit.
- **Add a new chapter to `ORDER`.**
- **A board ships with its file in `boards/`.**

## Read more

- [`DESIGN.md`](../DESIGN.md) for the architecture and the reasons for it.
- [`reference/`](../reference/README.md) for the period hardware manuals, as text.
- [Review comments](devguide/doc-review-comments.md) for how to leave a review note in a
  document.
