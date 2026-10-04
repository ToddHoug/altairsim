#!/bin/bash
#
# tools/user/mirror-watch.sh against a LIVE mirror -- the script a package holder runs to watch
# an assistant drive a machine (issue #664).
#
#   usage: mirror-watch.sh <altairsim> <mirror-watch.sh> <machine> <from> <text>
#
#     machine   a built-in machine whose ROM prints <text> with no media and no key
#     from      where `run` starts it (decimal)
#
# What a unit test of MirrorStream cannot tell you is whether THIS SCRIPT, started the way its
# README says, sees the guest. So this does what the person does, in the order that matters:
#
#   1. The watcher starts FIRST, with nothing listening. It must wait, not exit.
#   2. The simulator starts with --mcp --mirror, and an assistant (this script, over a pipe)
#      runs the guest. The watcher must connect and show what the guest prints -- from the
#      first byte, because the mirror accepts a watcher only while the guest runs.
#   3. The simulator stops. The watcher must say so and go back to waiting, not exit.
#
# The port is chosen by probing, and tried again on a collision: nothing can reserve it.
#
# bash, not sh: the script under test is bash (it connects through /dev/tcp), and this file
# finds its free port the same way. Nothing else is needed -- no nc, no expect, no python.

set -u

sim=$1; watch=$2; machine=$3; from=$4; text=$5

work=$(mktemp -d)
out=$work/watch.out
wpid=""
spid=""
cleanup() {
  [ -n "$wpid" ] && kill "$wpid" 2>/dev/null
  [ -n "$spid" ] && kill "$spid" 2>/dev/null
  rm -rf "$work"
}
trap cleanup EXIT

fail() {
  echo "mirror-watch: FAIL -- $*" >&2
  if [ -f "$out" ]; then
    echo "--- what the watcher showed ---" >&2
    cat "$out" >&2
    echo "-------------------------------" >&2
  fi
  exit 1
}

# wait_for <text> <seconds>: poll the watcher's output for a line of its own.
wait_for() {
  _n=$(( $2 * 10 ))
  while [ "$_n" -gt 0 ]; do
    grep -F -q "$1" "$out" 2>/dev/null && return 0
    sleep 0.1
    _n=$((_n - 1))
  done
  return 1
}

# ---- a bad argument is refused, with the usage line, before anything connects ---------------
usage=$("$watch" not-a-port 2>&1) && rc=0 || rc=$?
[ "$rc" -eq 1 ] || fail "'$watch not-a-port' exited $rc, not 1: $usage"
case $usage in
  *'is not a port number'*'Usage:'*) ;;
  *) fail "'$watch not-a-port' did not print its usage: $usage" ;;
esac

# ---- a free port, and the two programs that meet on it ---------------------------------------
# Not a fixed number: ctest runs tests in parallel, and a port something else owns would fail
# this test for a reason that has nothing to do with the script. A port that REFUSES a connection
# now is free now, but nothing holds it until the simulator binds it, and the watcher must be up
# first (step 1). Another test, or an outgoing connection, can take it in between (issue #695).
# So when the simulator says the port is in use, start over on another one. Any other failure
# is a real failure and stops the test at once.
#
# A printf into a FIFO whose reader has already exited would kill this script with SIGPIPE.
trap '' PIPE

listening=""
attempt=0
while [ "$attempt" -lt 5 ]; do
  attempt=$((attempt + 1))

  port=""
  try=0
  while [ "$try" -lt 50 ]; do
    cand=$(( 20000 + (RANDOM % 20000) ))
    if ! { exec 3<>"/dev/tcp/localhost/$cand"; } 2>/dev/null; then port=$cand; break; fi
    exec 3<&-
    try=$((try + 1))
  done
  [ -n "$port" ] || fail "found no free port in 50 tries"

  # ---- 1. the watcher first, with nothing to watch -------------------------------------------
  : > "$out"
  "$watch" "$port" > "$out" 2>&1 &
  wpid=$!
  wait_for "Nothing on localhost:$port yet. Waiting..." 10 ||
    fail "started before the simulator, the watcher did not say that it waits"
  kill -0 "$wpid" 2>/dev/null || fail "started before the simulator, the watcher exited"

  # ---- 2. the simulator, driven the way an assistant drives it -------------------------------
  # Through a FIFO held open, not a one-shot pipe: the watcher tries once a second, and a session
  # that is over in a few milliseconds would be gone before its next try. A real session lasts
  # minutes. So the simulator starts and LISTENS, the watcher connects, and only then does the
  # assistant run the guest -- which is also the order that proves the first byte is not lost.
  rm -f "$work/in"
  mkfifo "$work/in"
  "$sim" "$machine" --mcp --mirror "socket:$port" < "$work/in" > "$work/mcp.out" 2> "$work/mcp.err" &
  spid=$!
  exec 4> "$work/in"
  printf '%s\n' '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"mirror-watch-test","version":"0"}}}' >&4
  printf '%s\n' '{"jsonrpc":"2.0","method":"notifications/initialized"}' >&4

  # The simulator tries to listen before it reads its first request, and a failed listen is
  # reported on stderr while the session carries on. So its reply to `initialize` means the
  # listen has been tried: look at stderr then. (A "Connected" in the watcher proves nothing --
  # whatever took the port may be what the watcher reached.)
  n=150
  while [ "$n" -gt 0 ]; do
    kill -0 "$spid" 2>/dev/null || break
    grep -q '"id":1' "$work/mcp.out" 2>/dev/null && break
    sleep 0.1
    n=$((n - 1))
  done
  if grep -q 'Address already in use' "$work/mcp.err"; then
    exec 4>&-
    kill "$spid" 2>/dev/null
    wait "$spid" 2>/dev/null
    kill "$wpid" 2>/dev/null
    wait "$wpid" 2>/dev/null
    wpid=""
    spid=""
    continue
  fi
  kill -0 "$spid" 2>/dev/null || fail "the simulator exited at start: $(cat "$work/mcp.err")"
  grep -q '"id":1' "$work/mcp.out" 2>/dev/null ||
    fail "the simulator did not answer initialize: $(cat "$work/mcp.err")"
  listening=1
  break
done
[ -n "$listening" ] || fail "the simulator could not listen on any of 5 ports: $(cat "$work/mcp.err")"

wait_for "Connected to localhost:$port." 15 || {
  exec 4>&-
  fail "the simulator listens, and the watcher did not connect: $(cat "$work/mcp.err")"
}

printf '%s\n' "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"tools/call\",\"params\":{\"name\":\"run\",\"arguments\":{\"from\":$from,\"until\":\"$text\",\"timeout_ms\":20000}}}" >&4

# The watcher shows the guest WHILE the simulator is still up -- not only after it has gone.
wait_for "$text" 25 || {
  exec 4>&-
  fail "the watcher did not show what the guest printed ('$text')"
}

# Closing the pipe is what stops the simulator.
exec 4>&-
wait "$spid"
src=$?
[ "$src" -eq 0 ] || fail "the simulator exited $src: $(cat "$work/mcp.err")"
grep -F -q "$text" "$work/mcp.out" ||
  fail "the guest never printed '$text' to the assistant: $(cat "$work/mcp.out")"

# ---- 3. the simulator is gone; the watcher says so and stays ---------------------------------
wait_for "The connection ended. Waiting for the mirror to come back..." 10 ||
  fail "after the simulator stopped, the watcher did not say that the connection ended"
kill -0 "$wpid" 2>/dev/null || fail "after the simulator stopped, the watcher exited"

echo "mirror-watch: PASS"
