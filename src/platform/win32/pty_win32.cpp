// The Windows half of platform/pty.h: there is none. Windows has no pseudo-terminal
// that a terminal program can open as a COM port, so the sink is refused -- by name,
// with the thing that does work.

#include "platform/pty.h"

namespace altair::platform {

std::unique_ptr<Pty> openPty(const std::string&, std::string& err) {
    err = "pty is not available on Windows; use socket:PORT";
    return nullptr;
}

bool havePty() { return false; }

std::unique_ptr<PtyPeer> openPtyPeer(const std::string&, std::string& err) {
    err = "pty is not available on Windows";
    return nullptr;
}

}  // namespace altair::platform
