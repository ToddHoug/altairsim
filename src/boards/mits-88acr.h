#pragma once
//
// MITS 88-ACR -- the Audio Cassette Recorder interface (docs/boards/mits-88acr.md).
//
// THE CARD IS AN 88-SIO B WITH A MODEM BOLTED TO IT, AND THAT IS NOT AN ANALOGY. The
// whole of it -- the 88-SIO B half, the FSK modem, the tape transport, MOUNT/WIND/REWIND/
// EXTRACT, the WAV codec and the snapshot -- is the reusable CassetteBoardBase
// (boards/cassette-board-base.h), because the 88-UIO carries the same cassette section
// and inherits it as a SIBLING off that base rather than off this card.
//
// So an 88-ACR is a CassetteBoardBase that adds nothing but its name and its one modem's
// modulation (the base's default): no board built on another board, and a change to this
// concrete card cannot reach the 88-UIO, which used to be built on it.

#include "boards/cassette-board-base.h"

#include <string>

namespace altair {

class AcrBoard : public CassetteBoardBase {
public:
    std::string type() const override { return "acr"; }
};

} // namespace altair
