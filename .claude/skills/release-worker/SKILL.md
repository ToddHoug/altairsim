---
name: release-worker
description: Build, test, package and deliver ONE altairsim release archive on THIS machine — for a tag someone else created. Use when asked to build the release package, build vX.Y.Z, run the release build on this box, or do DISTRIBUTION.md §4.2. A build machine never tags, never publishes, never picks a version.
---

# Build one release archive on this machine

You are a **build machine**. You build one archive for this machine's platform, prove it, and
hand it to the coordinator. You **never** tag, push, create or publish a release, run `gh`, or
decide a version number. If you are asked to do any of that, that is the `release-coordinator`
skill, and it runs on the coordinator only.

**Every check below has a STOP. If a check fails, stop and report it — exact output included.
Do not work around it, do not decide it is probably fine, and do not deliver.** Nothing checks
the package after you hand it over; the next stop is a user's download.

## Before you start: what you need to be told

- **The version**, `X.Y.Z`. The tag is `vX.Y.Z`. If you were not given it, STOP and ask. Never
  guess it from `git tag`, `CMakeLists.txt` or the latest release.
- **Site values**, from `distribution.conf` in the repo root if it exists (see
  `distribution.conf.example` for every key), otherwise from whoever asked you — usually the
  coordinator, over ssh. Keys are prefixed by this machine's target in capitals
  (`LINUX_X86_64_PARALLEL`):
  - `<TARGET>_PARALLEL` — `no` means build without `--parallel`. If unknown, use `--parallel`
    only on a machine with 16 GB or more.
  - `<TARGET>_SERIAL_A` / `_SERIAL_B` — the null-modem pair, if this box has one.
  - `COORDINATOR_USER`, `COORDINATOR_HOST`, `COORDINATOR_DIST` — where step 8 delivers. If you
    do not have them, do steps 1–7 and report that you did not deliver.

**Your target** — find it, do not assume it:

| this machine | target | archive |
|---|---|---|
| macOS, `uname -m` = `arm64` | `macos-arm64` | `.tar.gz` |
| macOS, `uname -m` = `x86_64` | `macos-x86_64` | `.tar.gz` |
| Windows | `windows-x86_64` | `.zip` |
| Linux, `x86_64` | `linux-x86_64` | `.tar.gz` |

**Your shell does not keep environment variables between commands.** Anything a command needs
(`PATH`, `ALTAIR_SERIAL_A`) is set *in that same command*. If `cmake` is not found over a
non-login ssh shell, prefix the command with `export PATH="<TARGET>_PATH:$PATH"` (on an Intel
Mac that is `/usr/local/bin`).

## The eight steps — macOS and Linux

Run from the repo root.

```sh
# 1. The source AT THE TAG, never a branch.
#    --force and -f matter: a re-spin force-moves the tag, and a plain `git fetch --tags`
#    keeps the old one and silently builds the wrong commit.
git fetch --tags --force && git checkout -f vX.Y.Z

# 2. Configure, from a CLEAN build/. A reused build/ keeps its cached SDL3_DIR -- often
#    Homebrew's dylib -- and the configure below does not override it.
rm -rf build
cmake -B build -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_PREFIX_PATH="$HOME/opt/sdl3-static" \
      -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0          # macOS only; omit on Linux
```

**CHECK** the configure output contains `-- SDL3 found -- video boards enabled (windowed)`.
**STOP** if it says `-- SDL3 not found -- video boards build headless (null display)` — a
headless binary runs perfectly and draws nothing, and that is how v0.2.0 shipped. On macOS,
**never** pass `-DCMAKE_OSX_ARCHITECTURES`: each Mac builds native for itself.

```sh
# 3. Build. Drop --parallel if <TARGET>_PARALLEL=no (the compiler gets OOM-killed).
cmake --build build --config Release --parallel

# 4. Test -- everything except the slow CPU gate and the two tests that need hardware.
ctest --test-dir build -C Release -LE slow -E '^(serial-hw|tnfs-hw)$'
```

**CHECK** `100% tests passed out of N`. **STOP** on anything else. Read the pass line itself;
the absence of the word "error" proves nothing.

```sh
# 4b. The hardware tests -- WARN, never stop. Ports in the same command as ctest.
ALTAIR_SERIAL_A=<SERIAL_A> ALTAIR_SERIAL_B=<SERIAL_B> \
  ctest --test-dir build -C Release -R '^(serial-hw|tnfs-hw)$' --output-on-failure
```

**No ports configured? Leave the two variables off entirely.** An empty value is not the same
as unset: the test sees a variable, tries to open an empty path, and fails instead of skipping.

Report each of `serial-hw` and `tnfs-hw` as Passed, Skipped, or Failed. **If serial ports are
configured for this box and `serial-hw` is not Passed, warn loudly** (the cable, the ports,
or the serial code is wrong) — and carry on. `tnfs-hw` runs when `de-tnfsd` is on `PATH` and
skips otherwise; a skip is a note, a failure is a loud warning. Neither stops the release.

```sh
# 5. The binary knows exactly what it is.
./build/altairsim --version
```

**CHECK** exactly `AltairSim X.Y.Z`. **STOP** on a `-N-gsha` suffix (not on the tag) or
`(modified)` (dirty tree) — either means the binary cannot be traced to the release.

```sh
# 6. Package, with CI's manual handed in. The tag's tree already holds it; pandoc never runs.
tools/build-package.sh --pdf docs/altairsim-manual.pdf --target <target>

# 7. Prove the manual true against the ARCHIVE (extracts it outside the repo and runs the
#    manual's own `$ altairsim ...` commands).
tools/verify-package.sh dist/altairsim-X.Y.Z-<target>.tar.gz
```

**STOP** if `build-package.sh` refuses (headless binary, SDL linked by absolute path — it says
which). **CHECK** step 7 prints `verify-package: PASS -- N/N manual commands reached their
documented prompt`. **STOP** on any FAIL.

```sh
# 8. Deliver. The worker's only credential is the delivery key; it never touches GitHub.
scp dist/altairsim-X.Y.Z-<target>.tar.gz \
    <COORDINATOR_USER>@<COORDINATOR_HOST>:<COORDINATOR_DIST>/
```

Skip step 8 if you are the coordinator: your archive is already in `dist/`.

## Windows

Steps 1–5 in **PowerShell**, 6–8 in **Git Bash** (`build-package.sh` is `/bin/sh`). **No
Developer shell is needed:** the Visual Studio generator lets MSBuild find the toolchain. Do
not use Ninja for a release — it needs `vcvars` in the same command, every time. **MSVC is the
only supported Windows toolchain.**

```powershell
git fetch --tags --force; git checkout -f vX.Y.Z
Remove-Item -Recurse -Force build -ErrorAction SilentlyContinue
cmake -B build -G "Visual Studio 18 2026" -DCMAKE_BUILD_TYPE=Release `
      -DCMAKE_PREFIX_PATH="$env:USERPROFILE\opt\sdl3-static" `
      -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded
cmake --build build --config Release --parallel
ctest --test-dir build -C Release -LE slow -E '^(serial-hw|tnfs-hw)$'
# with ports; without, drop the two $env: assignments (an empty value fails, it does not skip)
$env:ALTAIR_SERIAL_A='<SERIAL_A>'; $env:ALTAIR_SERIAL_B='<SERIAL_B>'; ctest --test-dir build -C Release -R '^(serial-hw|tnfs-hw)$' --output-on-failure
.\build\Release\altairsim.exe --version
```

```sh
# Git Bash, repo root
tools/build-package.sh --pdf docs/altairsim-manual.pdf --target windows-x86_64
tools/verify-package.sh dist/altairsim-X.Y.Z-windows-x86_64.zip
scp dist/altairsim-X.Y.Z-windows-x86_64.zip <COORDINATOR_USER>@<COORDINATOR_HOST>:<COORDINATOR_DIST>/
```

Same CHECKs and STOPs as above. Windows-specific:

- **Name the generator.** Without `-G "Visual Studio 18 2026"` CMake picks the newest Visual
  Studio *it* knows, and an old CMake builds with 2022 silently. CI builds with 2026.
- **`MultiThreaded` is load-bearing.** It links the C runtime statically, so the `.exe` does
  not need the VC++ redistributable. SDL3 was built the same way; mixing the two fails to link.
- **`--config Release` is load-bearing** — the generator is multi-config. The binary lands in
  `build\Release\`; `build-package.sh` finds it there.
- **`verify-package` skips the pty checks on Windows** (there is no `expect`). A PASS line
  with those SKIPs is the expected result, not a failure.
- **The `.zip` is made by Windows' own `tar.exe`.** If it ever fails, do not fall back to
  `Compress-Archive`: PowerShell 5.1 writes backslashes that Unix `unzip` cannot extract.

## Report back

One block, whether you finished or stopped:

```
target        linux-x86_64
version       AltairSim X.Y.Z                (the --version line, verbatim)
tests         100% tests passed out of N     (verbatim)
serial-hw     Passed | Skipped | Failed      (+ ports used)
tnfs-hw       Passed | Skipped | Failed
SDL3          3.4.12                          (cat ~/opt/sdl3-static/.altairsim-sdl3-version)
verify        verify-package: PASS -- N/N ... (verbatim)
archive       altairsim-X.Y.Z-<target>.tar.gz  sha256 <hash>
delivered     yes | no -- <why>
stopped at    <step and the exact output>     (only if you stopped)
```

The SDL3 version matters: nothing makes the four machines agree, so this report is the only
record, and it is the first thing to check if a video bug shows on one platform only.

## First time on this machine

Done once, not at release time. If any is missing, STOP and report which.

- **A C++20 compiler, CMake ≥ 3.20, git.** No pandoc, no Chrome — the PDFs come from CI.
- **A checkout of the public repo over anonymous https**, no login and no token:
  `git clone https://github.com/deltecent/altairsim.git`, or on an existing clone
  `git remote set-url origin https://github.com/deltecent/altairsim.git`.
- **Static SDL3**, which the configure above expects at that prefix:
  `tools/build-sdl3-static.sh` (macOS, Linux) or `tools\build-sdl3-static.bat` (Windows).
  Idempotent. On Linux install the X11/Wayland headers first — `docs/building-linux.md` has the
  list, and `libxtst-dev` is required.
- **Windows:** `tools\windows\RUN-ME-setup-windows-worker.bat` installs and checks all of it
  (Visual Studio 2026 Build Tools, Git + Git Bash, OpenSSH Server, static SDL3).
- **Linux:** build on the oldest glibc you can — the binary will not start on a distro older
  than the one it was built on.
- **The delivery key.** A passphrase-less ed25519 key, `~/.ssh/altairsim_deploy`, used for
  nothing else, its public half in the coordinator's `~/.ssh/authorized_keys`, and in
  `~/.ssh/config`:

  ```
  Host <COORDINATOR_HOST>
      IdentityFile ~/.ssh/altairsim_deploy
      IdentitiesOnly yes
  ```

  It must have no passphrase because step 8 runs unattended. It writes to `dist/` and grants
  no GitHub access at all.
