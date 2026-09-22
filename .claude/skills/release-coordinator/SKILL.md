---
name: release-coordinator
description: Cut an altairsim release from the coordinator — bump the version, curate the changelog, tag the CI PDF commit, open the draft GitHub release, drive the build machines over ssh, collect the four archives, checksum, upload and publish. Use when asked to cut, tag, ship or publish a release, or to run DISTRIBUTION.md §5. Runs only on the machine holding the GitHub credentials.
---

# Cut a release, from the coordinator

**The coordinator is the only machine with credentials.** It tags, pushes, runs every `gh`
command, and collects the archives in its repo's `dist/`. The build machines hold no GitHub
credentials: they clone the public repo over anonymous https, build, and `scp` one archive
here. The coordinator is also a build machine for its own target.

**Read `distribution.conf` first** (repo root, gitignored; `distribution.conf.example` names
every key). It says which target this box builds (`COORDINATOR_TARGET`), how to reach each
worker (`<TARGET>_SSH`, `_REPO`, `_PARALLEL`, `_PATH`), their serial ports, and which targets
share one physical machine (`SHARED_HOST`). If it is missing, STOP and ask for it — do not
reconstruct addresses from memory or old docs.

The four targets: `macos-arm64`, `macos-x86_64`, `windows-x86_64`, `linux-x86_64`.

Steps 1–4 can be done ahead of build day. **Ask before every step that pushes, tags or talks
to GitHub** — those are outward-facing and hard to undo.

## 1. Bump the version and write the changelog

- `project(altairsim VERSION X.Y.Z …)` in `CMakeLists.txt` is the only place the number
  lives; everything else derives from it or from `git describe`.
- In the same commit, add the `## X.Y.Z` section to `docs/changelog/changelog.md`. **Curate,
  do not rename.** A released section is a short themed narrative in the voice of the sections
  already there (a handful of entries). `## Unreleased` is a longer working scratch that also
  carries things already shipped. Build the section from `git log --merges <prevtag>..HEAD`,
  group it into themes, and drop anything already released: `git merge-base --is-ancestor
  <sha> <prevtag>` true means it shipped. Do not trust `git log --grep` for this. Then leave a
  fresh, empty `## Unreleased`.
- Branch, PR, merge on green CI, as every change here.

## 2. Wait for CI's PDFs, and tag THAT commit

`docs.yml` rebuilds the documents on master and commits them as *"Rebuild the PDFs for
`<sha>`"*. **Tag that commit, not the merge** — otherwise the tagged tree carries a stale
manual, changelog, monitor or debugger PDF. Never build the PDFs locally: a different pandoc
is a different document.

## 3. Tag and push

```sh
git tag vX.Y.Z <pdf-rebuild-sha> && git push origin vX.Y.Z
```

This fires `cpu-exerciser-release.yml` (8080EXM, ZEXDOC, ZEXALL on all three CI platforms).
**Wait for it to go green before publishing anything.**

## 4. Open the draft

```sh
gh release create vX.Y.Z --draft --notes-file <notes>
```

## 5. Build on all four machines

Each machine runs the **`release-worker`** skill's steps. The coordinator runs them locally
for `COORDINATOR_TARGET` (its archive lands straight in `dist/`, no scp) and drives each
worker over ssh.

- **Any order, but never two targets in `SHARED_HOST` at once** — they are one physical
  machine, and building both just makes them fight over one CPU and one pool of RAM. Others
  can build alongside.
- **Pass each worker its site values in the command**: `--parallel` or not, `PATH` prefix,
  and `ALTAIR_SERIAL_A/B` if it has ports. A worker needs no config file of its own.
- **Tell each worker the version.** It never decides one.

Driving a worker over ssh — these have all bitten:

- **Never `ssh host 'bash -s' < script`.** Commands in the script inherit the script as stdin
  and desync bash's parser: the build passes, then a phantom syntax error, non-zero exit, no
  delivery. `scp` the script over and run it as a file: `ssh host 'bash /tmp/worker.sh'`.
- **Non-login shells skip the profile.** On an Intel Mac `cmake` is in `/usr/local/bin`,
  which is then not on `PATH`. Prefix with `export PATH="<TARGET>_PATH:$PATH"`.
- **Windows, PowerShell steps:** `ssh host 'powershell -NoProfile -ExecutionPolicy Bypass
  -File build-win.ps1'`. The default policy blocks a `.ps1`.
- **Windows, Git Bash steps:** bash is not on the ssh `PATH`. Use the space-free short path so
  it parses in cmd or PowerShell: `ssh host 'C:\PROGRA~1\Git\bin\bash.exe --login
  /c/Users/<user>/package-win.sh'`.
- **A moved tag needs `git fetch --tags --force`** on every worker, or it silently builds the
  old commit.

Collect each worker's report block. Any STOP is a STOP for the release: do not upload a set
with a hole in it. `serial-hw` and `tnfs-hw` results are warnings — put them in front of the
person, do not hold the release for them.

### Re-spinning for a doc-only fix

While the release is **still a draft**, a doc fix after tagging is shipped by force-moving the
tag so the archives still stamp a clean `X.Y.Z`: `git tag -f vX.Y.Z <sha>; git push --force
origin vX.Y.Z`, then rebuild on every box. Never move a published tag.

## 6. Verify, checksum, upload, publish

1. **Verify the archives** — the `release-verify` skill, on each of the four. Hand its human
   checklist to a person.
2. **Checksum:** `tools/build-checksums.sh`. It refuses unless all four archives of one
   version are in `dist/`, writes `dist/SHA256SUMS`, and checks it before returning.
3. **Upload all five:**
   ```sh
   gh release upload vX.Y.Z dist/altairsim-X.Y.Z-*.tar.gz dist/altairsim-X.Y.Z-*.zip dist/SHA256SUMS
   ```
4. **Publish:** `gh release edit vX.Y.Z --draft=false`.

That is the end of the release here. The website's download copy is refreshed **on the web
server itself**, from the published GitHub release — not from this machine.

## The two traps that shaped this

- **Build at the tag, not the merge.** v0.2.0's first binaries reported
  `AltairSim 0.2.0 (v0.1.0-86-g59dbba8)`: CI built the merge, an ancestor of the tag, and
  `git describe` found the previous tag. Every worker checks `--version` before packaging.
- **The manual in the package is the one CI built.** A local pandoc makes a different PDF.
  `--pdf docs/altairsim-manual.pdf` hands in the tagged tree's copy.
