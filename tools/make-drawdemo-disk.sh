#!/bin/sh
#
# Build cpm22b23-56k-drawdemo.dsk: the stock CP/M floppy, with room made for DRAWDEMO.COM.
#
#     tools/make-drawdemo-disk.sh
#
# WHY A FLOPPY. drawdemo was developed on an 8 MB hard-disk image, which is far too big to
# track. The CADzilla test disk is the same 330 KB 8" floppy as tests/media/cpm instead --
# cpm22b23-56k.dsk, with R/W/HDIR already on it (tools/install-hostbridge-utils.sh). That disk
# has 18K free and DRAWDEMO.COM is 18K, so room is made first: MBASIC.COM (24K) and the four
# BASIC programs that need it (LUNAR, STARINS, STARTRK, TICTAK -- 36K) come off. 78K free.
#
# THE TRACKED IMAGE ALREADY IS THE RESULT. This script is how it was made, and the recipe that
# makes the blob auditable rather than mysterious. It never writes to tests/media/cpm: it works
# on a copy, and replaces tests/media/cadzilla/cpm22b23-56k-drawdemo.dsk only once every check
# passes.
#
# THE PROGRAM ARRIVES AS ITS .HEX, and LOAD makes the .COM ON THE DISK, the way the program was
# built in the first place (CP/M's own ASM and LOAD). R fetches DRAWDEMO.HEX off the host through
# the host bridge, LOAD turns it into DRAWDEMO.COM, and the 51K hex is erased again -- the disk
# has room for both at once, but only the .COM stays.
#
# Then the result is booted AGAIN, from scratch, and W writes DRAWDEMO.COM back out to the host.
# That proves the file is on the disk, that it reads back, and that it is byte for byte the
# program this script expects (EXPECT below). A DIR listing would prove only the first.

set -eu

root=$(cd "$(dirname "$0")/.." && pwd)
ex=$root/tests/media/cadzilla          # the fixture: its HEX in, its disk out
sim=${ALTAIRSIM:-$root/build/altairsim}   # a caller with its own binary says so
src=$root/tests/media/cpm/cpm22b23-56k.dsk
out=$ex/cpm22b23-56k-drawdemo.dsk

# The SHA-256 of the DRAWDEMO.COM that LOAD makes from DRAWDEMO.HEX. If the program is rebuilt,
# commit the new .ASM and .HEX and change this line with them.
EXPECT=2c31535afb916a859351cfb71bc62e5a87b383fb78842600dc9a78f1b18dad37

[ -x "$sim" ] || { echo "make-drawdemo-disk: no $sim -- build first." >&2; exit 1; }
[ -f "$src" ] || { echo "make-drawdemo-disk: no $src" >&2; exit 1; }
[ -f "$ex/DRAWDEMO.HEX" ] || { echo "make-drawdemo-disk: no $ex/DRAWDEMO.HEX" >&2; exit 1; }

sha256() {
  if command -v sha256sum >/dev/null 2>&1; then sha256sum "$1" | cut -d' ' -f1
  else shasum -a 256 "$1" | cut -d' ' -f1; fi
}

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

CR=$(printf '\r')

cp "$src" "$work/work.dsk"
mkdir -p "$work/host"
cp "$ex/DRAWDEMO.HEX" "$work/host/"

# A BARE CR IN FRONT OF EVERY COMMAND -- a lightning rod. The BIOS re-initialises the 2SIO on
# each warm boot, and the byte waiting there when a program exits is lost. The rod gets eaten
# and the command behind it survives (tools/install-hostbridge-utils.sh has the whole story).
{
  printf '%s' "$CR"; printf 'ERA MBASIC.COM\r'
  printf '%s' "$CR"; printf 'ERA *.BAS\r'
  printf '%s' "$CR"; printf 'R DRAWDEMO.HEX\r'
  printf '%s' "$CR"; printf 'LOAD DRAWDEMO\r'      # -> DRAWDEMO.COM
  printf '%s' "$CR"; printf 'ERA DRAWDEMO.HEX\r'
  printf '%s' "$CR"; printf 'STAT\r'

  # THIS BIOS FLUSHES ITS TRACK BUFFER ON CONSOLE INPUT, not on close (docs/boards/mits-dcdd.md).
  # The last write reaches the image only when CP/M reads the keyboard again -- so leave it
  # keystrokes to do that with.
  i=0; while [ $i -lt 10 ]; do printf '%s' "$CR"; i=$((i+1)); done
} > "$work/keys"

cat > "$work/cmd" <<EOF
SET CONSOLE UPPER=OFF
SET hb0 HOSTDIR=$work/host
MOUNT dsk0:drive0 "$work/work.dsk"
RUN FF00
EOF
"$sim" -s "$work/cmd" < "$work/keys" > "$work/log" 2>&1 || true

# DID IT LAND? Boot the new image and ask it: W the program back out, and list what should be gone.
mkdir -p "$work/back"
{
  printf '%s' "$CR"; printf 'W DRAWDEMO.COM BACK.COM\r'
  printf '%s' "$CR"; printf 'DIR MBASIC.COM\r'
  printf '%s' "$CR"; printf 'DIR *.BAS\r'
  printf '%s' "$CR"; printf 'DIR *.HEX\r'
  i=0; while [ $i -lt 10 ]; do printf '%s' "$CR"; i=$((i+1)); done
} > "$work/vkeys"
cat > "$work/vcmd" <<EOF
SET CONSOLE UPPER=OFF
SET hb0 HOSTDIR=$work/back
MOUNT dsk0:drive0 "$work/work.dsk"
RUN FF00
EOF
"$sim" -s "$work/vcmd" < "$work/vkeys" > "$work/vlog" 2>&1 || true

ok=yes
if [ ! -f "$work/back/BACK.COM" ]; then
  echo "  FAIL  DRAWDEMO.COM did not come back off the disk" >&2; ok=no
elif [ "$(sha256 "$work/back/BACK.COM")" != "$EXPECT" ]; then
  echo "  FAIL  DRAWDEMO.COM on the disk is not the expected program" >&2
  echo "        got $(sha256 "$work/back/BACK.COM")" >&2
  echo "        want $EXPECT" >&2
  ok=no
else
  echo "  ok    DRAWDEMO.COM  (reads back off the disk, SHA-256 $EXPECT)"
fi

# Each DIR above must answer "No file" (this CP/M's spelling): MBASIC, the BASIC programs and
# the hex are all gone.
nofile=$(grep -a -c 'No file' "$work/vlog" || true)
if [ "$nofile" -ne 3 ]; then
  echo "  FAIL  expected 3 x 'No file' (MBASIC.COM, *.BAS, *.HEX), got $nofile" >&2; ok=no
else
  echo "  ok    MBASIC.COM, *.BAS and DRAWDEMO.HEX are gone"
fi

if [ "$ok" != yes ]; then
  echo "  $out was NOT modified. transcripts: $work/log, $work/vlog" >&2
  trap - EXIT                         # keep the evidence
  exit 1
fi

grep -a 'Space:' "$work/log" | tail -1 | sed 's/^/  /' || true
cp "$work/work.dsk" "$out"
echo "  wrote $out"
