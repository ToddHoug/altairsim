#!/bin/sh
# CP/M OFF A REAL TNFS SERVER -- the live half of tests/test_tnfs.cpp.
#
# test_tnfs.cpp proves the TNFS client against a fake server held in RAM: every packet, every
# retransmit, deterministic. What it cannot prove is that we and a real tnfsd agree on the
# wire. This boots the shipped CP/M example with its floppy mounted over UDP from de-tnfsd,
# then writes to that floppy and reads the write back through a second mount.
#
# de-tnfsd is POSIX-only and is not ours to build, so this test RUNS WHEN de-tnfsd IS ON PATH
# and SKIPs (77) when it is not -- installing it is what turns the test on. Labelled `hw`,
# like serial-hw: it touches the real world (a server process, the kernel's UDP stack).
#
# Every mount serves a COPY of the image from a scratch directory, never the tracked one.
#
# Usage: tnfs-hw.sh <altairsim> <source dir> <work dir>

sim=$1 src=$2 work=$3

if ! command -v de-tnfsd >/dev/null 2>&1; then
  echo "SKIPPED: de-tnfsd is not on PATH."
  echo "  This test boots CP/M off a real TNFS server. Install de-tnfsd and put it on PATH."
  exit 77
fi

image=cpm22b23-56k.dsk
rm -rf "$work" && mkdir -p "$work/srv" "$work/machine" || exit 1

server=
cleanup() { [ -n "$server" ] && kill "$server" 2>/dev/null; }
trap cleanup EXIT INT TERM

fail() {
  echo "tnfs-hw: $1"
  [ -n "$2" ] && { echo "--- output ---"; echo "$2"; }
  echo "--- de-tnfsd log ---"; cat "$work/tnfsd.log" 2>/dev/null
  exit 1
}

# Start de-tnfsd on a free port. There is no portable way to ask the kernel for a free UDP port
# from sh, so try a few and keep the first one the server says it is listening on.
start_server() {   # $1 = --serve-root or --serve-root-rw
  cleanup; server=
  cp "$src/examples/cpm/$image" "$work/srv/$image" || exit 1
  for try in 1 2 3 4 5; do
    port=$(( 20000 + ($$ * 7 + try * 1009) % 40000 ))
    de-tnfsd "$1" -p "$port" "$work/srv" >"$work/tnfsd.log" 2>&1 &
    server=$!
    for wait in 1 2 3 4 5 6 7 8 9 10; do
      grep -q "listening port=$port" "$work/tnfsd.log" 2>/dev/null && break
      kill -0 "$server" 2>/dev/null || break
      sleep 0.2
    done
    grep -q "listening port=$port" "$work/tnfsd.log" && return 0
    kill "$server" 2>/dev/null; server=
  done
  fail "de-tnfsd never started listening (tried five ports)"
}

# The machine: the shipped CP/M example, with drive 0 mounted over TNFS instead of from disk.
write_machine() {
  cat >"$work/machine/cpm-tnfs.toml" <<EOF
[machine]
name = "cpm22-tnfs"
base = "default"
startup = ["RUN FF00"]

[console]
bsdel = "bs"

[[board]]
id = "dsk0"
  [[board.drive]]
  unit  = 0
  mount = "tnfs://127.0.0.1:$port/$image"
EOF
}

# Run the machine with keys piped in. The first key is a NUL because CP/M's cold start eats
# one byte; the trailing CRs give the guest time to finish the last command, because a piped
# run stops the machine the moment input ends.
run() {   # $1 = keys (printf format)
  printf "$1" >"$work/keys"
  (cd "$work/machine" && "$sim" cpm-tnfs.toml <"$work/keys" 2>&1)
}

expect() {   # $1 = output, $2 = what must appear, $3 = what that proves
  case "$1" in *"$2"*) ;; *) fail "'$2' never reached the terminal: $3" "$1" ;; esac
}

# ---- 1. BOOT, READ-ONLY SERVER. --------------------------------------------------------
# The server refuses writes, so the disk mounts write-protected -- and CP/M must still boot
# and read its directory across the network.
start_server --serve-root
write_machine
out=$(run '\000\rDIR\r\r\r\r')
for want in '56K CP/M 2.2b v2.3' 'A>' 'A: L80      COM'; do
  expect "$out" "$want" "CP/M did not boot off tnfs://127.0.0.1:$port/$image"
done
cmp -s "$work/srv/$image" "$src/examples/cpm/$image" ||
  fail "the read-only server's image changed"

# ---- 2. WRITE BACK, READ/WRITE SERVER. -------------------------------------------------
# SAVE a file, quit (the image is written back to the server), then mount it afresh and find
# the file. The second mount starts from what the server holds, so finding the file proves
# the write crossed the network and landed.
start_server --serve-root-rw
write_machine
out=$(run '\000\rSAVE 1 TNFSHW.COM\r\r\r\r')
expect "$out" 'A>SAVE 1 TNFSHW.COM' "the SAVE was never typed"
cmp -s "$work/srv/$image" "$src/examples/cpm/$image" &&
  fail "SAVE ran but the server's image did not change -- nothing was written back" "$out"
out=$(run '\000\rDIR TNFSHW.COM\r\r\r\r')
expect "$out" 'A: TNFSHW   COM' "the saved file is not on the server's copy of the disk"

echo "tnfs-hw: PASS -- CP/M booted off de-tnfsd, and a SAVE came back from the server"
