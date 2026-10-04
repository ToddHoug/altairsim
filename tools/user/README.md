# The watch scripts

Two scripts that show the console of the machine in a second window, while an AI assistant
controls the machine. Start the simulator with a mirror:

```
altairsim examples/cpm/cpm22-buffered.toml --mcp --mirror socket:2323
```

Then run one script in a second window:

| Script | Runs on | Needs |
|---|---|---|
| `mirror-watch.sh` | macOS, Linux | bash only |
| `mirror-watch.ps1` | Windows | PowerShell only |

Each script connects to the mirror itself. You do not need `nc` or `telnet`.

A script waits if the mirror is not there yet. It shows the output while it is connected. When
the simulator stops, the script waits, and it connects again when the simulator starts again.
You can leave the window open for a full session.

Press Ctrl-C to stop a script.

## On macOS and Linux

```
tools/mirror-watch.sh
```

## On Windows

Windows does not run a PowerShell script file by default. There are two ways to run this one.

**One time.** This command changes no setting:

```
powershell -ExecutionPolicy Bypass -File tools\mirror-watch.ps1
```

**Each time, for your account.** Run this command once:

```
Set-ExecutionPolicy -Scope CurrentUser RemoteSigned
```

Windows marks a file that came from a web browser, and the files of a zip that came from a web
browser. Remove the mark once:

```
Unblock-File tools\mirror-watch.ps1
```

After that, `tools\mirror-watch.ps1` runs directly.

## Arguments

The two scripts use the same arguments:

| Arguments | Watches |
|---|---|
| (none) | `localhost:2323` |
| `altairsim` | `localhost:2323` |
| `port` | `localhost:port` |
| `host port` | `host:port`, for a mirror on a different computer |

Port 2323 is the port that the manual uses in its examples. If you start the simulator with a
different port, give the same port to the script.

## Limits

- **The scripts only watch.** They do not send the keys that you type. To type on a mirror, use
  `nc localhost 2323`.
- **One watcher at a time.** The mirror accepts one connection. Open only one watch window for
  each mirror.
- **The mirror accepts a watcher only while the machine runs.** A window can show `Connected`
  and nothing more until the assistant runs the machine. If the window stays empty while the
  machine runs, look for a second watcher that has the mirror.

## Credit

The scripts were written by trgeuy for serial-mcp-server
(https://github.com/trgeuy/serial-mcp-server). They have the MIT license. The license text is
in `LICENSE-mirror-watch.txt`.
