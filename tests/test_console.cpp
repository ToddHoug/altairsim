#include "test.h"

#include "core/value.h"
#include "host/console.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

using namespace altair;

namespace {

// Find the named property on the one Console. All of log/attn/base/history and the
// transform chain come through here, so this is exactly the seam SET CONSOLE and MCP
// use -- driving it is driving the real thing.
Property prop(const std::string& name) {
    for (Property& p : Console::instance().properties())
        if (p.name == name) return p;
    // A missing property is a hard failure of the test, not a silent empty.
    CHECK(false, "console property exists");
    return Property{};
}

std::string setLog(const std::string& path, std::string& err) {
    Property p = prop("log");
    return p.set(Value::ofStr(path), err) ? std::string("ok") : std::string("fail");
}

std::string readFile(const std::filesystem::path& p) {
    std::ifstream f(p, std::ios::in | std::ios::binary);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// Drive bytes to the screen the way the guest does -- through Console::write(), so they
// pass the whole filter chain (all off by default but bell) down to writeRaw(), which is
// where the log tap lives. Plain ASCII so no default filter touches it.
void screen(const std::string& s) {
    Console::instance().write(reinterpret_cast<const uint8_t*>(s.data()), s.size());
    Console::instance().flush();
}

} // namespace

void test_console() {
    SECTION("console log tees screen output to a file, and off stops it");

    namespace fs = std::filesystem;
    const fs::path dir = fs::temp_directory_path();
    const fs::path logp = dir / "altair_console_log_test.log";
    std::error_code ec;
    fs::remove(logp, ec);  // start clean, ignore "not there"

    std::string err;

    // OPEN. A fresh path, opened for append on an absent file, is just a create.
    CHECK(setLog(logp.string(), err) == "ok", "SET log=<path> succeeds");
    CHECK(err.empty(), "and reports no error");
    CHECK(prop("log").get().s() == logp.string(), "SHOW CONSOLE would read the path back");

    // TEE. Exactly the bytes that reached the screen land in the file.
    const std::string first = "MEMORY SIZE? 4096\r\n";
    screen(first);
    CHECK(readFile(logp) == first, "the file holds exactly what went to the screen");

    // OFF. `off` closes the file; nothing further is recorded.
    CHECK(setLog("off", err) == "ok", "SET log=off succeeds");
    CHECK(prop("log").get().s().empty(), "and clears the path");
    const std::string after = "THIS MUST NOT BE LOGGED\r\n";
    screen(after);
    CHECK(readFile(logp) == first, "the file did not grow after log=off");

    SECTION("empty path also closes, and re-opening appends");

    // Re-open the same path: APPEND, so the earlier transcript is preserved.
    CHECK(setLog(logp.string(), err) == "ok", "re-open the same log");
    const std::string more = "SECOND SESSION\r\n";
    screen(more);
    CHECK(readFile(logp) == first + more, "append kept the first session and added the second");

    // The empty string is the other way to say 'off'.
    err.clear();
    CHECK(setLog("", err) == "ok", "SET log= (empty) closes");
    CHECK(prop("log").get().s().empty(), "empty path leaves logging off");

    SECTION("an unopenable path is reported and leaves logging off");

    err.clear();
    // A file inside a directory that does not exist cannot be opened for append.
    const std::string bad = (dir / "no_such_dir_altairsim" / "x.log").string();
    CHECK(setLog(bad, err) == "fail", "SET log=<bad path> fails");
    CHECK(!err.empty(), "and returns an error message");
    CHECK(prop("log").get().s().empty(), "and logging stays off");
    screen("STILL NOT LOGGED\r\n");  // must not throw and must go nowhere

    fs::remove(logp, ec);

    // ---------------------------------------------------------------------
    // PASTE -- a file behind the keyboard buffer, read as the guest takes bytes
    // ---------------------------------------------------------------------
    // The buffer holds 256 bytes and inject() drops what does not fit. A paste must
    // not: the file waits behind the buffer, so its size has no limit and no byte of
    // it is lost or reordered.
    SECTION("paste: a file far larger than the buffer arrives whole and in order");
    {
        Console& con = Console::instance();
        uint8_t  b;
        while (con.read(&b, 1)) {}  // start from an empty buffer

        // A pasted byte passes the console's input transforms, like a typed key. This
        // test is about the STREAM, so switch them off for it and put them back after
        // (the one Console is shared with every other suite).
        const Value oldBsdel = prop("bsdel").get();
        const Value oldUpper = prop("upper").get();
        const Value oldStrip = prop("strip7in").get();
        CHECK(prop("bsdel").set(Value::ofStr("off"), err) &&
                  prop("upper").set(Value::ofBool(false), err) &&
                  prop("strip7in").set(Value::ofBool(false), err),
              "input transforms off for the byte-exact checks");

        // 100,000 bytes, every value 0..255 in it -- so ATTN (0x05) is in there too.
        std::string big(100000, '\0');
        for (size_t i = 0; i < big.size(); ++i) big[i] = (char)((i * 7 + i / 251) & 0xFF);
        const fs::path bigp = dir / "altair_console_paste_test.bin";
        {
            std::ofstream f(bigp, std::ios::binary);
            f.write(big.data(), (std::streamsize)big.size());
        }

        const uint64_t droppedBefore = con.dropped();
        err.clear();
        CHECK(con.pasteFile(bigp.string(), err), "the file opens");
        CHECK(con.pasting(), "and a paste is now waiting");
        CHECK(con.pending() == big.size(), "every byte of it counts as pending");

        std::string got;
        got.reserve(big.size());
        while (con.read(&b, 1)) got += (char)b;
        CHECK(got.size() == big.size(), "all 100,000 bytes reached the guest");
        CHECK(got == big, "in order, unchanged -- 8-bit clean");
        CHECK(con.dropped() == droppedBefore, "and not one was dropped");
        CHECK(!con.takeAttn(), "a 0x05 inside the file is data, not the ATTN key");
        CHECK(!con.pasting() && con.pending() == 0, "at end of file the paste is gone");

        SECTION("paste: text typed after a paste arrives after it; before it, at once");
        const std::string head = big.substr(0, 1000);
        const fs::path    headp = dir / "altair_console_paste_head.bin";
        {
            std::ofstream f(headp, std::ios::binary);
            f.write(head.data(), (std::streamsize)head.size());
        }
        CHECK(con.pasteFile(headp.string(), err), "a 1,000-byte file");
        con.typeText("EX 0000\r");
        got.clear();
        while (con.read(&b, 1)) got += (char)b;
        CHECK(got == head + "EX 0000\r", "the file first, then the typed line");

        con.typeText("now");
        CHECK(!con.pasting() && con.pending() == 3, "with no paste, typed text is in the buffer at once");
        got.clear();
        while (con.read(&b, 1)) got += (char)b;
        CHECK(got == "now", "and reads back");

        // THE KEYBOARD COMES FIRST. A paste that filled the keyboard buffer left no room
        // for a real key -- the terminal was not even asked, so ATTN was dead for the
        // whole file, and a key from the window was dropped. So: a key pressed during a
        // paste is not dropped, and the guest reads it BEFORE the rest of the file.
        SECTION("paste: a key pressed during a paste is kept, and goes ahead of the file");
        CHECK(con.pasteFile(bigp.string(), err), "a paste is running");
        for (int i = 0; i < 5; ++i) con.read(&b, 1);  // ...and the guest is part-way in
        const uint64_t droppedMid = con.dropped();
        con.inject(std::string(200, 'K'));            // 200 keys from the operator
        CHECK(con.dropped() == droppedMid, "none of the keys was dropped -- the buffer is the keyboard's");
        got.clear();
        for (int i = 0; i < 201; ++i) {
            con.read(&b, 1);
            got += (char)b;
        }
        CHECK(got == std::string(200, 'K') + big[5], "the 200 keys first, then the file where it left off");
        CHECK(con.cancelPaste() == big.size() - 6, "and the rest of the file is still waiting");

        SECTION("paste: cancel drops the rest and says how much");
        CHECK(con.cancelPaste() == 0, "nothing pasted: nothing to drop");
        CHECK(con.pasteFile(bigp.string(), err), "paste the big file again");
        con.typeText("tail");
        for (int i = 0; i < 300; ++i) con.read(&b, 1);  // the guest takes 300 bytes
        CHECK(con.cancelPaste() == big.size() + 4 - 300, "the count is everything not yet read");
        CHECK(!con.pasting() && con.pending() == 0, "and nothing more arrives");
        CHECK(con.read(&b, 1) == 0, "the keyboard is empty");
        CHECK(con.pasteFile(bigp.string(), err), "paste once more");
        con.inject("abc");
        CHECK(con.cancelPaste() == big.size(), "a cancel counts the file only");
        CHECK(con.pending() == 3, "and leaves the keys the operator typed");
        while (con.read(&b, 1)) {}

        SECTION("paste: a file that will not open is refused and queues nothing");
        err.clear();
        CHECK(!con.pasteFile((dir / "no_such_dir_altairsim" / "x.ent").string(), err),
              "a missing file fails");
        CHECK(!err.empty(), "with a reason");
        err.clear();
        CHECK(!con.pasteFile(dir.string(), err) && !err.empty(), "and so does a directory");
        CHECK(!con.pasting() && con.pending() == 0, "nothing was queued");

        // An empty file is a paste of nothing, not an error.
        const fs::path emptyp = dir / "altair_console_paste_empty.bin";
        { std::ofstream f(emptyp, std::ios::binary); }
        CHECK(con.pasteFile(emptyp.string(), err) && !con.pasting(), "an empty file pastes nothing");

        prop("bsdel").set(oldBsdel, err);
        prop("upper").set(oldUpper, err);
        prop("strip7in").set(oldStrip, err);

        fs::remove(bigp, ec);
        fs::remove(headp, ec);
        fs::remove(emptyp, ec);
    }
}
