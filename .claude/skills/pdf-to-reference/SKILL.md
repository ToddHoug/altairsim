---
name: pdf-to-reference
description: Turn a scanned period manual or data sheet (a PDF) into a distilled reference/*.md — triage the text layer, read the register and bit tables as page images, write the distillation, and add the two mandatory catalog rows in reference/README.md and docs/sources.md. Use when asked to read a PDF and add it to reference/, convert or distill a manual or data sheet, or document a board from its scan.
---

# Distill a PDF into `reference/<name>.md`

The deliverable is a **text-only distillation** of a period scan: register maps, port
addresses, status/control bit tables, geometry and timing, boot sequences, and the quirks
that bite an implementer. Not a summary of the manual — what a developer would reopen the
scan for.

**The scan itself stays untracked.** `.gitignore` allows `reference/*.md` and nothing else
under `reference/`. Never `git add` the PDF. Work on a branch off `master`; `work-task` and
`ship-change` still own the gates.

## 1. Triage the text layer

```sh
pdfinfo "X.pdf" | grep -iE 'pages|creator|producer'
pdftotext "X.pdf" - | wc -c
```

- **Near zero characters** (tens, not thousands) ⇒ a pure page-image scan. Skip `pdftotext`
  entirely and read pages as images.
- **Lots of characters but dirty** (0/O and 1/l/I flipped, "Crolllellleo" = Cromemco) ⇒
  hybrid. Use the text to *navigate* and to draft prose; verify **every number** against the
  page image.
- **Clean text layer** ⇒ still read bit tables as images (step 3).

Hybrid navigation:

```sh
pdftotext -layout "X.pdf" "$SCRATCH/x.txt"
grep -niE 'port|command|register|bit|status|track|sector|interrupt' "$SCRATCH/x.txt"
```

## 2. The tools that exist on this box

- **poppler** — `pdfinfo`, `pdftotext`, `pdftoppm` at `/opt/homebrew/bin`. Present.
- **The Read tool** — renders PDF pages **visually** (`pages: "12-20"`, max 20 per call).
  This is the workhorse. No OCR involved: you read the page image and transcribe.
- **NOT available**, whatever an old note claims: `tesseract`, and python `fitz`/PyMuPDF,
  `pypdf`, `pdfplumber` are not importable in the system `python3`. Do not reach for them.

Work out the **page offset** early (PDF page N ↔ printed page N−k, from the footer) and cite
*printed* page numbers in the `.md`.

## 3. Read every table as a page image

Always. OCR flips exactly the characters a register map depends on. Transcribe command
letters, port addresses, bit positions and switch settings from what you *see*.

Watch for a scan that is **incomplete without saying so** (the 88-SIO "Rev 0 & 1" scan has no
Rev 1 section), and for **handwritten marginalia** — a previous owner's corrections are
evidence, not noise. Mark them as marginalia, and say whether they won.

## 4. When the manual contradicts itself

It will. Record **both readings**, say **which one won**, and say **what settled it**. The
tiebreaker order:

1. **The manual's own shipped software** — driver listings, sample programs, boot PROMs. They
   ran on the hardware; the paragraph did not.
2. A period artifact we already have — a monitor ROM, a tape, a disk image, disassembled.
3. Arithmetic that closes (a bit rate that divides to the quoted byte time).
4. Another manual in `reference/`, when the same chip is documented twice.

Never average two readings, and never quietly pick one. Mark every unresolved conflict ⚠.

## 5. Write `reference/<basename>.md`

Same basename as the PDF (spaces are fine; the catalog links percent-encode them). Shape,
following the existing files (`reference/88-VI-RTC.md` is a good model):

```
# <Vendor> <Model> — <what it is>

Source: [<exact pdf filename>](#)          <- the `#` is a placeholder URL

<one paragraph: vendor, date, page count, what the board/chip is, what this file omits>
<say plainly if it is **Not emulated**>

## 1. Quick reference for emulation      <- a table an implementer can work from
## 2..n  <sections: port map, register/bit tables, sequences, geometry, timing, interrupts,
         hardware/parts identification, shipped software>
## Quirks worth carrying                 <- the ⚠ list; last section
```

Plain-text Markdown tables, no images, no source paths outside the repo. Chip-level detail
that a *separate* chip already covers (a 6850, an MSM5832, an 8253) is **deferred to that
file by link**, not duplicated — split a chip out into its own `reference/*.md` if the board
manual is the only place it is documented.

## 6. Catalog — MANDATORY, both files

A distillation that is not indexed is invisible. Both rows, in the same commit:

- **`reference/README.md`** — a row in the section that fits (add a new `## <section>` only
  if none does), `| [Title](Percent%20Encoded%20Name.md) | what it covers |`. One dense
  paragraph: what the thing is, the numbers that matter, the ⚠ traps, and whether it is
  emulated.
- **`docs/sources.md`** — a manifest row in `## The manifest`:
  `| \`file.pdf\` | what it is (title, ©/date, page count, **text-layer status**, provenance
  URL + fetch date, notable scan defects) | authoritative for (the `.md` path, the source
  files or board docs it backs, the key facts, the ⚠ traps) |`.
- If the conversion cost you a **"paid for once" lesson**, add it to `## Traps, paid for
  once` in `docs/sources.md` too.

## 7. Provenance — ask, do not invent

Authorized sources: **deramp.com**, **altairclone.com**, **s100computers.com**; bitsavers for
vendor data sheets; **righto.com** only for silicon die reverse-engineering. Period manuals
and first-hand artifacts only — **never read another emulator's source** (SIMH/AltairZ80
included) to learn how hardware works.

If the PDF arrived in Patrick's `~/Downloads` with no stated origin, **ask him for the URL**
before filling the manifest. Do not guess a path, and do not write "origin unknown" when a
question would settle it.

## 8. Finish

- `git status` shows only the three tracked files. The PDF is **not** added.
- `reference/` is not compiled and no test covers it — there is nothing to build or run.
  Verification is the spot-check: every number in the `.md` traceable to the page it came
  from.
- Stop for the maintainer's review (`work-task`), then `ship-change`.
