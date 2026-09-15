#pragma once
//
// 88-SIO -- MITS Serial I/O Board. ONE serial port, one COM2502 UART.
// See docs/boards/mits-88sio.md.
//
// THE CARD THAT INVERTS ITS STATUS BITS. The 88-2SIO's status register reads
// TRUE (bit set = condition true); the 88-SIO's ready bits read INVERTED (bit
// CLEAR = ready). A machine can have both cards in it, both conventions live at
// once, and a driver written for one silently misbehaves on the other. They share
// no code, and that is on purpose -- see the comment over SerialBoardBase::statusByte().
//
// THIS IS THE ENGINE, PLAINLY. The whole of the 88-SIO B -- the UART, the inverted
// status word, the VI straps, the interrupt enables, the port decode, the connector
// and the snapshot -- is SerialBoardBase (boards/serial-board-base.h), because the
// 88-ACR and the 88-UIO carry the same PCB and inherit it as SIBLINGS off that base
// rather than off this card. So an 88-SIO is a SerialBoardBase that adds nothing but
// its name: no board built on another board, and a change to this concrete card cannot
// reach the ACR or the UIO.

#include "boards/serial-board-base.h"

#include <string>

namespace altair {

class SioBoard : public SerialBoardBase {
public:
    std::string type() const override { return "sio"; }
};

} // namespace altair
