#pragma once
//
// A pseudo-terminal, for the `pty` mirror sink (issue #683) -- and, later, a `pty`
// endpoint (issue #685).
//
// The same contract as serial.h and socket.h: pure declarations, no OS type in any
// signature, one implementation file per OS. POSIX has pseudo-terminals; Windows has
// nothing a terminal program can open as a port, so its openPty() REFUSES.
//
// WHAT IT IS FOR. The simulator holds the MASTER side. A person opens the SLAVE side
// with a terminal program (`screen`, `minicom`, `cu`), which sends each key as it is
// typed and does not echo it -- what a guest console wants, and what `nc` on a socket
// does not give.
//
// THE NAME IS A SYMBOLIC LINK. The kernel picks the device name (/dev/ttys004,
// /dev/pts/3) and it differs at every start; /dev itself is root's. So the stable name
// is a link the simulator makes and removes: `/tmp/altairsim{n}` (the first free n), or
// a path the operator gives.
//
// NON-BLOCKING THROUGHOUT, like the socket: a line is pumped every slice and must
// never stall emulated time.

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace altair::platform {

class Pty {
public:
    virtual ~Pty() = default;

    // Look at the line: is a program on the slave side? Call it from pump(). On the
    // edge where a program arrives, the slave is put in RAW mode before anything is
    // sent -- a freshly opened slave echoes, and an echo of the guest's output would
    // come back as typed keys.
    virtual void poll() = 0;

    // A program has the slave side open (as of the last poll()).
    virtual bool attached() const = 0;

    // Bytes the program typed. 0 when there are none, or nobody is there.
    virtual size_t read(uint8_t* buf, size_t n) = 0;

    // Returns what it TOOK: less than n when the kernel buffer is full, 0 when nobody
    // is there. The caller keeps or drops the rest -- it is never a reason to wait.
    virtual size_t write(const uint8_t* buf, size_t n) = 0;

    // The kernel's name for the slave side, and the link that points at it.
    virtual std::string device() const = 0;
    virtual std::string link() const = 0;
};

// Make a pseudo-terminal and its link. `linkPath` empty picks the first free
// `/tmp/altairsim{n}`. A given path replaces a symbolic link that is already there and
// refuses anything else. Null (and `err` set) on failure -- and always on Windows.
std::unique_ptr<Pty> openPty(const std::string& linkPath, std::string& err);

// Does this host have pseudo-terminals at all? False on Windows. For a caller that must
// know BEFORE it tries -- a test that has nothing to check where there is no pty.
bool havePty();

// THE OTHER SIDE: the slave, as a terminal program holds it. The simulator never opens
// its own slave; this is here so a test can be the program on the far end without
// reaching for the OS itself (DESIGN.md 2.1 holds for tests too).
class PtyPeer {
public:
    virtual ~PtyPeer() = default;
    virtual size_t read(uint8_t* buf, size_t n) = 0;   // non-blocking; 0 when quiet
    virtual size_t write(const uint8_t* buf, size_t n) = 0;
    virtual bool   echoes() const = 0;  // is the line in echo mode (not raw)?
};

// Open a slave by its device name or by a link to it. Never becomes the caller's
// controlling terminal. Null (and `err` set) on failure -- and always on Windows.
std::unique_ptr<PtyPeer> openPtyPeer(const std::string& path, std::string& err);

}  // namespace altair::platform
