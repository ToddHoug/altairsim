#pragma once
//
// `telnet:PORT` / `telnet:HOST:PORT` -- a socket line that speaks the Telnet
// protocol (RFC 854), so a stock `telnet` client behaves like a real terminal
// server instead of a raw pipe. DESIGN.md 7.1, 7.7.
//
// WHY THIS EXISTS, AND WHY IT IS NOT `socket:`. A `socket:` endpoint is a RAW
// byte pipe -- that is exactly right for machine-to-machine `CONNECT` and for the
// live mirror, and it must stay that way. But when a human points `telnet` at a
// raw socket, nothing negotiates the terminal, so the client stays in its default
// LINE mode with LOCAL echo on: every keystroke is echoed by the client AND again
// by the guest (a BBS echoes what it receives), and Enter arrives as a whole line,
// CR mangled to LF. That double-echo is not a modem bug -- it is a missing telnet
// handshake, and this decorator supplies it.
//
// A TelnetStream WRAPS a TcpStream (the same decorator shape as TeeStream and
// MirrorStream) and adds only the protocol layer:
//
//   * On each new session (carrier rising edge) it OFFERS the options a terminal
//     server offers -- WILL ECHO, WILL SUPPRESS-GO-AHEAD, DO SUPPRESS-GO-AHEAD --
//     which is what makes a stock client turn OFF its local echo and go
//     character-at-a-time. A dial-OUT (client) offers the mirror image: it asks
//     the far end to echo (DO ECHO / DO SGA) and never offers to echo itself.
//   * It STRIPS inbound IAC commands so the guest never sees a negotiation byte,
//     answering each with the refusing/agreeing verb ONLY when that changes our
//     advertised state -- which is what keeps the negotiation loop-free.
//   * It DOUBLES a data 0xFF on the way out (telnet requires it) and folds the
//     client's `CR NUL` / `CR LF` down to a bare CR on the way in, which is what
//     the guest's console code expects from a keypress.
//
// Everything else -- carrier, CTS backpressure, DTR-hangup, the line rate, SHOW --
// is the wrapped TcpStream's, forwarded verbatim. describe() echoes `telnet:...`,
// so SHOW and CONFIG SAVE round-trip it.

#include "host/stream.h"

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace altair {

class TelnetStream : public ByteStream {
public:
    // `inner` is the raw TCP line (a TcpListenStream or TcpConnectStream); `spec` is
    // the operator's `telnet:...` text, echoed by describe(). `server` is true for a
    // LISTEN (we host a BBS, so WE offer to echo) and false for a dial-out (we are
    // the client, so we ask the far end to echo).
    TelnetStream(std::unique_ptr<ByteStream> inner, std::string spec, bool server);

    std::string describe() const override { return spec_; }

    size_t read(uint8_t* buf, size_t n) override;
    size_t write(const uint8_t* buf, size_t n) override;

    // Readability is about DECODED data, not raw bytes: the far end may have sent
    // nothing but a negotiation, which the parser eats whole in pump().
    bool readable() const override { return !rxClean_.empty(); }
    bool writable() const override { return inner_->writable(); }
    bool pacesItself() const override { return inner_->pacesItself(); }
    bool pacedReceive() const override { return inner_->pacedReceive(); }

    void flush() override { inner_->flush(); }
    void pump() override;

    // The far end's pins, the card's pins, the line rate and the operator log are
    // all the wrapped socket's -- telnet negotiation rides in the data stream and
    // changes none of them.
    LineStatus status() const override { return inner_->status(); }
    void       setControl(const LineControl& c) override { inner_->setControl(c); }
    bool       setParams(const LineParams& p, std::string& err) override {
        return inner_->setParams(p, err);
    }
    std::vector<std::string> drainLog() override { return inner_->drainLog(); }

private:
    void onConnect();                              // a fresh session: reset + offer options
    void consume(const uint8_t* buf, size_t n);    // run inbound bytes through the NVT parser
    void respond(uint8_t verb, uint8_t opt);       // negotiate one option, loop-free

    std::unique_ptr<ByteStream> inner_;
    std::string                 spec_;
    bool                        server_;

    std::string rxClean_;     // decoded data waiting for the guest
    std::string pendingOut_;  // telnet commands to send (RAW: their 0xFF is protocol)

    bool wasUp_ = false;      // carrier edge, to spot a new session and renegotiate

    // NVT input parser state, persisted across pump() calls so a command split over a
    // buffer boundary is still parsed correctly.
    enum class St { Data, Iac, Opt, Sub, SubIac };
    St      st_        = St::Data;
    uint8_t verb_      = 0;      // the DO/DONT/WILL/WONT awaiting its option byte
    bool    lastWasCR_ = false;  // fold the CR's LF/NUL tail

    // Per-option negotiation state -- what we have TOLD the far end about our options
    // (myState_) and asked of its (hisState_). Tri-state so "never mentioned" is
    // distinct from "told WONT"; we send a verb only when the target differs from what
    // we last told, which is the whole of the loop-avoidance.
    std::array<uint8_t, 256> myState_{};   // MyUnknown / MyWill / MyWont
    std::array<uint8_t, 256> hisState_{};  // HisUnknown / HisDo / HisDont
};

} // namespace altair
