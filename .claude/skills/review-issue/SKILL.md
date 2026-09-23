---
name: review-issue
description: How an incoming issue is reviewed — check it belongs on this tracker, read it with its comments, find the need behind the solution it proposes (and ask when that is unclear), check whether altairsim already serves it, reproduce a bug over --mcp, then post a verdict comment and a label. Use when asked to review, triage, look at or answer issue #N, or to go through the open issues. Ends at the comment; it never builds anything.
---

# Review an incoming issue

`review-pr` reviews a contributor's pull request. **This skill reviews an issue** — a bug
report or a feature request, usually from someone who is not the maintainer.

It ends at **a comment, a label and a report.** It never branches, never edits the tree,
never plans the change. If a verdict says something should be built, that is a fresh
`work-task`, started by the maintainer.

The rules, set by the maintainer:

- **I post the comment myself**, signed `--AltairSim Claude`. No draft for approval.
- **I never close the issue.** The person who opened it does. No `Closes #N`.
- **I never quote the maintainer.**
- **An unclear need is a question, not a verdict.** I ask; I do not decide for them.

## 0. Does it belong on this tracker?

Before anything else. This repository's issues are about **the simulator and its release
package**. Common misfilings:

| It is really about | Where it belongs |
|---|---|
| altairsim.com, the downloads page, the website | **Discussions** (they are enabled) |
| CP/M, MBASIC, DDT, M80 — software running *inside* the guest | upstream; not ours |
| "how do I…", with no defect and no request | Discussions, or answer it and label `question` |

**Say so kindly and once.** A misfiled issue is still someone taking the trouble to write;
the comment points at the better place, it does not scold.

**An issue is often half-misfiled.** #461 asks for the Markdown manuals on the *website* and,
in a follow-up comment, *in the release package*. The first half is Discussions; the second
half is ours and gets a real review. When the two halves are both live, say so plainly and
carry on with the half that is ours — see step 5.

## 1. Read it — all of it

```sh
gh issue view <N> --json number,title,author,state,createdAt,labels,body,comments
gh issue list --state all --search "<the words they used>"
```

- **Read every comment.** The real ask is often not in the body: in #461 it arrived two weeks
  later, and it changed what the issue was about.
- **What version are they on?** A release, or master? It decides step 3.
- Read the related issues the search turns up, and the `DESIGN.md` section it touches.

## 2. Why? — the stated solution is not the need

An issue almost always arrives as **a solution**: "add a `--foo` flag", "ship a zip of the
Markdown manuals". Behind it is a problem. Write down one line each, and do not skip a line
because it feels obvious:

1. **What were they trying to do?** In their terms, not mine.
2. **What already serves that need** — possibly in a different form?
3. **What would the proposed thing add over that?**

**The inventory for line 2 is the release package, not the repository.** Someone holding a
release has what `tools/build-package.sh` lists at the top of the file, and nothing else.
That already includes, deliberately:

- the manual, monitor and debugger guides, the changelog, the cheat sheet, QUICK-START and
  migrating — **as PDFs, because most people cannot read Markdown**
- `DRIVING-WITH-AI.md` and `cheatsheet.md` — **plain text, put there for an AI assistant**
- `skills/altairsim/` — the Agent Skill, for a client that reads skills
- `examples/`, each a machine file with the media it mounts

**Shipping a second copy of shipped content in another format is a cost** — package size, and
one more thing that has to stay in step with the release, every release, forever. The issue
has to pay for it. "It would be convenient" does not.

### If line 3 is blank, ask — do not decide

**A need I cannot state is not a need I may rule on.** Not "needed", not "declined". The
comment goes back to the requester: name what already serves the need, and ask plainly what
it fails to do for them. Label `question`, verdict *need unclear — asked*, and the review
ends there until they answer. When they do, start again at step 1.

Guessing someone's intent on their behalf — and then deciding against the guess — is the
mistake this step exists to prevent.

## 3. A bug: reproduce it, don't argue with it

Drive the guest with `altairsim <machine> --mcp`. **Never hand-roll an expect script** —
`CLAUDE.md` says why, and the `altairsim` skill has the recipe.

- **Reproduce on the version they name**, then on `master`. Fixed since their release is its
  own verdict: say which release carries the fix.
- Cannot reproduce? Say exactly what was tried — machine file, commands, what appeared —
  so they can tell us what is different. *Cannot reproduce* is a request for detail, never a
  dismissal.
- **A confirmed bug is fixed, not documented.** If the comment is turning into "be careful
  to…", the verdict is a bug, and the fix is code.
- **Never give hardware a behavior it never had** to make a symptom go away. Check the host,
  the filter and the monitor layers first.

## 4. A feature: does it earn its place?

`work-task` §1a, applied to their issue. After step 2 has found the real need:

1. **Can altairsim already do it?** Search the manual, `HELP`, `docs/manual/ref/`, the machine
   files, the commands and properties.
   - **It can, and it is documented:** point at the section or `HELP` entry. No change.
   - **It can, but nothing says so, or it cannot be found:** a **documentation gap**. That is
     a real outcome with real work behind it — it is not a "no".
2. **Was it declined before?** Search closed issues and `DESIGN.md`. A decision already made
   is reported with its reason, not re-argued from scratch.
3. **Does it fit the design?** Read the `DESIGN.md` section. Hardware behavior that never
   existed is a no, however useful it would be.

## 5. The verdict

**One verdict, and it opens the comment.** From this set only:

| Verdict | Label |
|---|---|
| already works, and it is documented | `question` |
| already served in another form | `question` |
| documentation gap | `documentation` |
| confirmed bug | `bug` |
| needed, not yet built | `enhancement` |
| declined before — with the issue or `DESIGN.md` section | `wontfix` |
| not this tracker | `invalid` |
| cannot reproduce | `question` |
| duplicate of #N | `duplicate` |
| need unclear — asked | `question` |

Use the labels that exist; never invent one. Apply it with:

```sh
gh issue edit <N> --add-label <label>
```

**A mixed issue:** give the verdict for the half that is ours, say plainly that the other
half belongs elsewhere, and — when the two are genuinely separate pieces of work — file the
in-scope half as its own issue, as in `work-task` §3, and link it. Never let an out-of-scope
half swallow a real request.

## 6. Comment

```sh
gh issue comment <N> --body-file <file>
```

- **The verdict first**, in a sentence. Not at the end of three paragraphs.
- **Then the evidence**: the section, the `HELP` entry, the earlier issue, the release that
  carries the fix, the `file:line`.
- **Then what happens next**, if anything.
- Thank them for the report in one line, no more.
- Plain words. No hedging, no lecture, no tallies of what they got wrong.
- Signed `--AltairSim Claude`.
- **Do not close the issue.**

## 7. Report and stop

To the maintainer: the issue, the verdict, the label applied, the comment posted, and — for a
"needed" or a "documentation gap" — one line on the shape of the work, so they can decide
whether to start a `work-task`. **Do not start it.**

## Going through the backlog

Reviewing several at once: **oldest first**, one comment and one label per issue, each
through steps 0–6 on its own. No batching of comments, no summary comment on a single issue
standing in for the rest. The report at the end is one table: issue, verdict, label.

Stop and ask the maintainer if the same verdict is coming up over and over — that usually
means the docs have a hole, not that a dozen people are wrong.
