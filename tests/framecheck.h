#pragma once
//
// CHECK_FRAME -- assert on the whole picture a video board drew, headless.
//
// A test pumps a board against the NullDisplay and then says what the frame should look
// like as a TEXT GRID (host/framedump.h), one character per pixel, written as a raw string
// literal right there in the test so the expected picture is readable by a person. On a
// mismatch the check prints the first row that differs, expected over actual with a caret
// under the column, AND writes the actual frame as a .ppm into the temp directory, naming
// the path in the failure so it can be opened at once. A text grid says WHERE it went
// wrong; the image says WHAT it drew.
//
// Same shape as CHECK(): it counts a run and, on failure, a fail and a FAIL line.

#include "test.h"

#include "host/display_null.h"
#include "host/framedump.h"

#include <cstdint>
#include <functional>

// Compare `owner`'s last frame on `d` against `expected`, a grid exactly as frameText()
// produces it under `o` (rows separated by '\n'). One leading newline is ignored so a
// raw string literal may open on its own line, and a missing final newline is supplied.
// `what` names the check in the failure and in the .ppm's filename.
bool checkFrame(altair::NullDisplay& d, altair::Display::Owner owner, const char* expected,
                const altair::TextGridOpts& o, const char* what);

// The same check against a golden FILE, tests/golden/<name>.txt in the source tree, for a
// frame too large to keep inline. With ALTAIR_TEST_WRITE_GOLDEN=1 in the environment the
// actual frame is written there instead and the check passes -- an explicit, deliberate
// act, because a golden that rewrites itself asserts nothing.
bool checkFrameGolden(altair::NullDisplay& d, altair::Display::Owner owner, const char* name,
                      const altair::TextGridOpts& o, const char* what);

#define CHECK_FRAME(disp, owner, expected, what) \
    CHECK(checkFrame((disp), (owner), (expected), altair::TextGridOpts{}, (what)), (what))

#define CHECK_FRAME_OPTS(disp, owner, expected, opts, what) \
    CHECK(checkFrame((disp), (owner), (expected), (opts), (what)), (what))

#define CHECK_FRAME_GOLDEN(disp, owner, name, opts, what) \
    CHECK(checkFrameGolden((disp), (owner), (name), (opts), (what)), (what))

// EVERY PIXEL, against an oracle. A sampled text grid proves the geometry in a form a
// person can read, but it looks at one pixel in a thousand of a 640x480 frame. When the
// test knows exactly what it drew it can say what EVERY pixel should be -- a rectangle's
// perimeter is four comparisons, a line one -- and this compares all of them. On a
// mismatch it reports the first differing pixel (x, y, expected, actual), how many
// differ in all, and writes the actual frame as a .ppm like the grid check does.
// `expected(x, y)` returns the palette index for frame pixel (x, y), row 0 at the top.
bool checkFramePixels(altair::NullDisplay& d, altair::Display::Owner owner,
                      const std::function<uint8_t(int x, int y)>& expected, const char* what);

#define CHECK_FRAME_PIXELS(disp, owner, expected, what) \
    CHECK(checkFramePixels((disp), (owner), (expected), (what)), (what))
