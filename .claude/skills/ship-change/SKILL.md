---
name: ship-change
description: How a reviewed altairsim change is shipped — commit after the maintainer approves, open the PR only after a second approval, poll CI, merge on green, then comment on the related issues. Use when told to commit, open the PR, push, ship or merge. Every step here waits for its own approval; work-task comes first.
---

# Ship a reviewed change: commit, PR, merge, issues

**Each gate is one explicit yes from the maintainer, for that step only.** An approval to
commit is not an approval to open a PR. If you are unsure whether you have one, you do not:
ask.

## 1. Commit — after the maintainer approves the review

- Stage exactly the files that were reviewed, by name. Never `git add -A` or `git add .`.
- The message follows the repo's style: `type(scope): summary`, then a body that says why.
- **No `Co-Authored-By`, no AI attribution, no quote of the maintainer.**
- If more changes are wanted, go back to `work-task` step 3. Every new change is reviewed
  before it is committed.

## 2. Wait for approval to open the PR

Report the commits on the branch (`git log --oneline master..`) and stop. More commits may be
wanted first; each goes through review and step 1.

## 3. Open the PR — after approval

```sh
git push -u origin <branch>
gh pr create --base master --title "<type(scope): summary>" --body-file <file>
```

The body gives a summary, the verification done (pass lines), and `Fixes #N` or `Refs #N`
for each related issue. No attribution. Any comment you post on it ends with
`--AltairSim Claude`.

## 4. Poll CI every 20 seconds

```sh
gh pr checks <N>
```

Wait until all three platforms (Linux, macOS, Windows) have finished.

- **Red:** stop and report the failing step and its log excerpt
  (`gh run view <id> --log-failed`). Never re-run a failure hoping it goes green — that hides
  a flake. Never merge. A fix for red CI is a new change: `work-task` step 3, review, commit
  approval.

## 5. Merge on green

No further approval is needed once all three are green.

```sh
gh pr merge <N> --merge --delete-branch
```

**Never `--auto`:** there is no branch protection, so it merges immediately, before CI.
Before merging, say so if the change leans on anything CI did not check — a platform not
tested by hand, a window check a person has not done.

## 6. After the merge

- **Comment on each related issue:** `Fixed by #<PR> (merged <sha>)`, or what is still left,
  signed `--AltairSim Claude`. **Never close an issue** — the person who opened it does.
- Sync: `git switch master && git pull --ff-only`.
- If `TODO.md` tracks the item, update it. It is untracked, so it needs no branch or PR.

## A PR from another contributor

If the PR's author is not `deltecent`, this skill does not apply: use `review-pr`. Its merge
needs green CI **and** the maintainer's approval.
