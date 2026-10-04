// The POSIX half of platform/pty.h (macOS and Linux).
//
// THREE THINGS HERE ARE NOT OBVIOUS, and each was found by running it:
//
//  1. "NOBODY THERE" IS ONLY VISIBLE AFTER SOMEBODY HAS BEEN. A master whose slave was
//     never opened reads EAGAIN -- the same answer as "open and quiet". Once a slave has
//     been opened and closed, poll() reports POLLHUP until the next open. So openPty()
//     PRIMES the line: it opens the slave and closes it again, and from then on POLLHUP
//     means nobody is there.
//
//  2. A SLAVE COMES UP COOKED, WITH ECHO -- and on macOS it goes BACK to that at every
//     reopen. Echo would return the guest's output to us as typed keys. Setting raw mode
//     on the MASTER reaches the pair, so poll() does it on every edge where a program
//     arrives, before a byte is sent. A terminal program sets its own mode anyway; this
//     is for the one that does not (`cat`).
//
//  3. THE LINK IS CLAIMED WITH symlink(), which fails if the name exists -- so two
//     simulators starting at once cannot both take /tmp/altairsim0.

#include "platform/pty.h"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

namespace altair::platform {
namespace {

void makeRaw(int fd) {
    termios t{};
    if (tcgetattr(fd, &t) != 0) return;
    cfmakeraw(&t);
    tcsetattr(fd, TCSANOW, &t);
}

// The slave is in echo or line mode. macOS puts it back there at EVERY open, so this is
// asked on every poll and not only when a program arrives: a program that closes the
// slave and opens it again between two polls shows no edge to see.
bool isCooked(int fd) {
    termios t{};
    return tcgetattr(fd, &t) == 0 && (t.c_lflag & (ECHO | ICANON)) != 0;
}

// A link left by a run that did not clean up: it points at a device that is gone, or
// at a slave whose master is closed. Opening the target tells us; O_NOCTTY so the probe
// can never become our controlling terminal.
bool linkIsLeftover(const std::string& path) {
    int fd = ::open(path.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) return true;
    ::close(fd);
    return false;
}

bool isSymlink(const std::string& path) {
    struct stat st{};
    return ::lstat(path.c_str(), &st) == 0 && S_ISLNK(st.st_mode);
}

bool exists(const std::string& path) {
    struct stat st{};
    return ::lstat(path.c_str(), &st) == 0;
}

class PosixPty : public Pty {
public:
    PosixPty(int master, std::string device, std::string link)
        : fd_(master), device_(std::move(device)), link_(std::move(link)) {}

    ~PosixPty() override {
        // Remove the link only while it is still OURS -- a later run may have replaced
        // a link it judged a leftover, and that one is not ours to take away.
        char    buf[1024];
        ssize_t n = ::readlink(link_.c_str(), buf, sizeof buf - 1);
        if (n > 0 && std::string(buf, (size_t)n) == device_) ::unlink(link_.c_str());
        ::close(fd_);
    }

    void poll() override {
        pollfd p{fd_, POLLIN, 0};
        bool   now = ::poll(&p, 1, 0) >= 0 && !(p.revents & POLLHUP);
        if (now && isCooked(fd_)) makeRaw(fd_);
        attached_ = now;
    }

    bool attached() const override { return attached_; }

    size_t read(uint8_t* buf, size_t n) override {
        if (!attached_) return 0;
        ssize_t r = ::read(fd_, buf, n);
        return r > 0 ? (size_t)r : 0;
    }

    size_t write(const uint8_t* buf, size_t n) override {
        if (!attached_) return 0;
        ssize_t w = ::write(fd_, buf, n);
        return w > 0 ? (size_t)w : 0;
    }

    std::string device() const override { return device_; }
    std::string link() const override { return link_; }

private:
    int         fd_;
    std::string device_;
    std::string link_;
    bool        attached_ = false;
};

class PosixPtyPeer : public PtyPeer {
public:
    explicit PosixPtyPeer(int fd) : fd_(fd) {}
    ~PosixPtyPeer() override { ::close(fd_); }

    size_t read(uint8_t* buf, size_t n) override {
        ssize_t r = ::read(fd_, buf, n);
        return r > 0 ? (size_t)r : 0;
    }
    size_t write(const uint8_t* buf, size_t n) override {
        ssize_t w = ::write(fd_, buf, n);
        return w > 0 ? (size_t)w : 0;
    }
    bool echoes() const override {
        termios t{};
        return tcgetattr(fd_, &t) == 0 && (t.c_lflag & ECHO) != 0;
    }

private:
    int fd_;
};

}  // namespace

bool havePty() { return true; }

std::unique_ptr<PtyPeer> openPtyPeer(const std::string& path, std::string& err) {
    int fd = ::open(path.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) {
        err = "pty: cannot open '" + path + "': " + std::strerror(errno);
        return nullptr;
    }
    return std::make_unique<PosixPtyPeer>(fd);
}

std::unique_ptr<Pty> openPty(const std::string& linkPath, std::string& err) {
    int master = ::posix_openpt(O_RDWR | O_NOCTTY);
    if (master < 0) {
        err = std::string("pty: cannot make a pseudo-terminal: ") + std::strerror(errno);
        return nullptr;
    }
    auto fail = [&](const std::string& what) {
        err = what;
        ::close(master);
        return nullptr;
    };
    if (::grantpt(master) != 0 || ::unlockpt(master) != 0)
        return fail(std::string("pty: cannot make a pseudo-terminal: ") + std::strerror(errno));
    const char* name = ::ptsname(master);
    if (!name) return fail("pty: the pseudo-terminal has no name");
    std::string device = name;

    // Prime (see the file header): one open and close of the slave, so that POLLHUP
    // means "nobody there" from the start. Raw now as well -- on Linux it stays.
    int slave = ::open(device.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (slave < 0) return fail("pty: cannot open " + device + ": " + std::strerror(errno));
    makeRaw(slave);
    ::close(slave);

    int fl = ::fcntl(master, F_GETFL);
    if (fl < 0 || ::fcntl(master, F_SETFL, fl | O_NONBLOCK) != 0)
        return fail(std::string("pty: cannot set non-blocking: ") + std::strerror(errno));
    ::fcntl(master, F_SETFD, FD_CLOEXEC);

    std::string link;
    if (linkPath.empty()) {
        // The first free number. symlink() is the claim: it fails with EEXIST if another
        // simulator took the name between our look and our try, and we move on.
        for (int n = 0; n < 100 && link.empty(); ++n) {
            std::string cand = "/tmp/altairsim" + std::to_string(n);
            if (exists(cand)) {
                if (!isSymlink(cand) || !linkIsLeftover(cand)) continue;
                ::unlink(cand.c_str());
            }
            if (::symlink(device.c_str(), cand.c_str()) == 0) link = cand;
        }
        if (link.empty()) return fail("pty: no free name from /tmp/altairsim0 to /tmp/altairsim99");
    } else {
        if (exists(linkPath)) {
            if (!isSymlink(linkPath))
                return fail("pty: '" + linkPath + "' is not a link; will not replace it");
            ::unlink(linkPath.c_str());
        }
        if (::symlink(device.c_str(), linkPath.c_str()) != 0)
            return fail("pty: cannot make the link '" + linkPath + "': " + std::strerror(errno));
        link = linkPath;
    }

    return std::make_unique<PosixPty>(master, device, link);
}

}  // namespace altair::platform
