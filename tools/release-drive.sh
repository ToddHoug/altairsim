#!/bin/sh
#
# The coordinator's side of a release build: start tools/release-worker.sh on one build
# machine, PROVE THAT IT STARTED, and read its result. It reads this site's machines from
# distribution.conf (distribution.conf.example names every key).
#
# WHY THIS IS A SCRIPT. How a worker is started over ssh was written by hand for each release,
# and it is where the mistakes were (issue #643, and the trap list that the
# `release-coordinator` skill used to carry):
#
#   - `ssh host 'bash -s' < script` gives the script to its own commands as stdin, and the
#     shell's parser loses its place. The script is COPIED to the box and run as a file.
#   - It is copied OUTSIDE the checkout: step 1 of the worker is `git checkout -f`, which can
#     replace a script inside the checkout while it runs.
#   - A foreground ssh that drops takes the build with it. On macOS and Linux the worker runs
#     under `nohup` on the box and writes its log there.
#   - Windows sshd ends a remote background process when the session ends. There the ssh
#     itself runs under a local `nohup`, with a keep-alive, and the log is local. In 1.3.0 a
#     `Start-Process` wrapper printed `started` and ran nothing.
#   - Bash is not on the ssh PATH of a Windows box. It is called by its short path, which has
#     no space in it: C:\PROGRA~1\Git\bin\bash.exe.
#
# AN EMPTY LOG PROVES NOTHING. `start` returns OK only when the checkout on the box is at the
# new tag AND the worker script is a running process there AND its log names this version.
#
#   usage: tools/release-drive.sh start  <target> X.Y.Z [--skip-slow] [--no-deliver]
#                                        [--repo DIR] [--conf FILE]
#          tools/release-drive.sh status <target> [--conf FILE]
#
#     <target>      macos-arm64 | macos-x86_64 | linux-x86_64 | windows-x86_64
#     --skip-slow   Do not run the CPU exercisers. For a dry run only.
#     --no-deliver  Do not copy the archive to the coordinator. For a dry run only.
#     --repo        Build in this checkout, not in the one that distribution.conf names. Use
#                   it for a dry run of the coordinator's own target in a second clone.
#     --conf        Read this file. Default: distribution.conf in the repository root.
#
# `status` prints the end of the worker's log. Its exit status is 0 when the worker printed
# `release-worker: OK`, 1 when it printed FAIL or is gone with no result, and 2 while it
# still runs.
#
# The coordinator's own target (COORDINATOR_TARGET) runs here, with no ssh and no delivery:
# its archive is written to dist/ directly. Its checkout must have no change to a tracked
# file, because the worker checks out the tag in it.

set -u

root=$(cd "$(dirname "$0")/.." && pwd)
worker=$root/tools/release-worker.sh
name=altairsim-release-worker
# The same name as a pgrep pattern that does not match the shell that runs the pgrep.
pat='[a]ltairsim-release-worker'
winbash='C:\PROGRA~1\Git\bin\bash.exe'
sshopt="-o BatchMode=yes -o ConnectTimeout=10"

die() { echo "release-drive: FAIL -- $*" >&2; exit 1; }

usage() {
  sed -n '/^#   usage:/,/^# file, because/p' "$0" | sed 's/^# \{0,1\}//'
  exit "${1:-0}"
}

[ $# -ge 1 ] || usage 1
cmd=$1; shift
case $cmd in
  start|status) ;;
  -h|--help) usage 0 ;;
  *) echo "release-drive: unknown command: $cmd" >&2; usage 1 ;;
esac

target=""
version=""
slow=yes
deliver=yes
repo_arg=""
conf=$root/distribution.conf

while [ $# -gt 0 ]; do
  case $1 in
    --skip-slow)  slow=no; shift ;;
    --no-deliver) deliver=no; shift ;;
    --repo)       [ $# -ge 2 ] || die "--repo needs a value"; repo_arg=$2; shift 2 ;;
    --conf)       [ $# -ge 2 ] || die "--conf needs a value"; conf=$2; shift 2 ;;
    -*)           echo "release-drive: unknown argument: $1" >&2; usage 1 ;;
    *)            if [ -z "$target" ]; then target=$1
                  elif [ -z "$version" ]; then version=$1
                  else echo "release-drive: unknown argument: $1" >&2; usage 1; fi
                  shift ;;
  esac
done

case $target in
  macos-arm64|macos-x86_64|linux-x86_64|windows-x86_64) ;;
  *) echo "release-drive: give a target." >&2; usage 1 ;;
esac

[ -f "$conf" ] || die "$conf is missing. Copy distribution.conf.example and fill it in; do not rebuild the addresses from memory"
# shellcheck disable=SC1090
. "$conf"

# The value of <TARGET>_<KEY> for a target, or empty.
conf_of() {  # $1 = target, $2 = KEY
  key=$(printf '%s' "$1" | tr 'a-z-' 'A-Z_')_$2
  eval "printf '%s' \"\${$key:-}\""
}

host=$(conf_of "$target" SSH)
repo=${repo_arg:-$(conf_of "$target" REPO)}
is_local=no
[ "$target" = "${COORDINATOR_TARGET:-}" ] && is_local=yes
if [ $is_local = no ] && [ -z "$host" ]; then
  die "$conf has no worker for $target"
fi
[ $is_local = no ] || [ -n "$repo" ] || repo=$root

mkdir -p "$root/dist"
log=$root/dist/worker-$target.log     # local targets and Windows: the log is here
pidf=$root/dist/worker-$target.pid
rscript=/tmp/$name.sh                 # macOS and Linux workers
rlog=/tmp/$name.log

# Is the worker of a target still running? 0 = yes.
running() {  # $1 = target
  t_host=$(conf_of "$1" SSH)
  if [ "$1" = "${COORDINATOR_TARGET:-}" ] || [ "$1" = windows-x86_64 ]; then
    # A local process: the worker itself, or the ssh that holds the Windows worker.
    [ -f "$root/dist/worker-$1.pid" ] && kill -0 "$(cat "$root/dist/worker-$1.pid")" 2> /dev/null
  else
    [ -n "$t_host" ] && ssh $sshopt "$t_host" "pgrep -f '$pat'" > /dev/null 2>&1
  fi
}

# The worker's log, to stdout.
fetch_log() {
  if [ $is_local = yes ] || [ "$target" = windows-x86_64 ]; then
    cat "$log" 2> /dev/null
  else
    ssh $sshopt "$host" "cat $rlog" 2> /dev/null
  fi
}

# `git describe --tags` in the worker's checkout.
describe() {
  if [ $is_local = yes ]; then
    git -C "$repo" describe --tags 2> /dev/null
  else
    ssh $sshopt "$host" "git -C $repo describe --tags" 2> /dev/null | tr -d '\r'
  fi
}

# Print the result line of a log and return 0 for OK, 1 for FAIL, 2 for none yet.
result_of() {
  line=$(printf '%s\n' "$1" | tr -d '\r' | grep -E '^release-worker: (OK|FAIL)' | tail -n 1)
  case $line in
    'release-worker: OK'*)   return 0 ;;
    'release-worker: FAIL'*) return 1 ;;
    *)                       return 2 ;;
  esac
}

# ---------------------------------------------------------------------------
if [ "$cmd" = status ]; then
  text=$(fetch_log)
  printf '%s\n' "$text" | tail -n 25
  result_of "$text"; res=$?
  case $res in
    0|1) exit $res ;;
  esac
  if running "$target"; then
    echo "release-drive: $target still runs."
    exit 2
  fi
  die "the worker of $target is not running and its log has no result line"
fi

# ---------------------------------------------------------------------------
# start
printf '%s\n' "$version" | grep -E '^[0-9]+\.[0-9]+\.[0-9]+$' > /dev/null \
  || die "give the version as X.Y.Z"

if running "$target"; then
  die "a worker of $target already runs"
fi

# Two targets that are one physical machine build one at a time.
case ",${SHARED_HOST:-}," in
  *",$target,"*)
    for other in $(printf '%s' "${SHARED_HOST:-}" | tr ',' ' '); do
      [ "$other" = "$target" ] && continue
      if running "$other"; then
        die "$other shares a machine with $target (SHARED_HOST) and still runs. Start $target when it is done"
      fi
    done ;;
esac

# The worker's arguments. The values of distribution.conf have no spaces (its format).
args="--version $version --repo $repo"
[ "$(conf_of "$target" PARALLEL)" = no ] && args="$args --no-parallel"
[ -n "$(conf_of "$target" PATH)" ] && args="$args --path $(conf_of "$target" PATH)"
if [ -n "$(conf_of "$target" SERIAL_A)" ] && [ -n "$(conf_of "$target" SERIAL_B)" ]; then
  args="$args --serial-a $(conf_of "$target" SERIAL_A) --serial-b $(conf_of "$target" SERIAL_B)"
fi
[ $slow = yes ] || args="$args --skip-slow"
if [ $is_local = no ] && [ $deliver = yes ]; then
  [ -n "${COORDINATOR_USER:-}" ] && [ -n "${COORDINATOR_HOST:-}" ] && [ -n "${COORDINATOR_DIST:-}" ] \
    || die "$conf does not have COORDINATOR_USER, COORDINATOR_HOST and COORDINATOR_DIST"
  args="$args --deliver $COORDINATOR_USER@$COORDINATOR_HOST:$COORDINATOR_DIST"
fi

if [ $is_local = yes ]; then
  copy=${TMPDIR:-/tmp}/$name.sh
  cp "$worker" "$copy" || die "cannot copy the worker script to $copy"
  # shellcheck disable=SC2086
  nohup sh "$copy" $args > "$log" 2>&1 < /dev/null &
  echo $! > "$pidf"
elif [ "$target" = windows-x86_64 ]; then
  # The copy goes to the login directory, which is also where Git Bash starts.
  scp $sshopt "$worker" "$host:$name.sh" > /dev/null \
    || die "cannot copy the worker script to $host"
  # shellcheck disable=SC2086
  nohup ssh $sshopt -o ServerAliveInterval=15 "$host" "$winbash --login $name.sh $args" \
    > "$log" 2>&1 < /dev/null &
  echo $! > "$pidf"
else
  scp $sshopt "$worker" "$host:$rscript" > /dev/null \
    || die "cannot copy the worker script to $host"
  ssh $sshopt "$host" "nohup sh $rscript $args > $rlog 2>&1 < /dev/null &" \
    || die "cannot start the worker on $host"
fi

# Prove it. The fetch and the checkout of step 1 take some seconds, so look for a minute.
tries=0
while [ $tries -lt 20 ]; do
  sleep 3
  tries=$((tries + 1))
  text=$(fetch_log)
  result_of "$text"
  if [ $? = 1 ]; then
    printf '%s\n' "$text" | tail -n 15
    die "the worker of $target stopped"
  fi
  # The first line of the worker names the version, so the log is of THIS run.
  if printf '%s\n' "$text" | grep -E "^release-worker: .*, v$version, in " > /dev/null \
     && [ "$(describe)" = "v$version" ] && running "$target"; then
    echo "release-drive: OK -- $target started. The checkout is at v$version and the worker runs."
    echo "  result:  tools/release-drive.sh status $target"
    exit 0
  fi
done

printf '%s\n' "$text" | tail -n 15
die "$target did not start: after 60 seconds the checkout is at '$(describe)', not v$version, or no worker process is there, or the log does not name v$version"
