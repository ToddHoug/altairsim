#include "test.h"

#include "host/level_pcm.h"

#include <cstdint>
#include <vector>

using namespace altair;

namespace {

// Sign changes in a run of samples: two for each cycle of a tone. A zero sample (an edge
// that fell in the middle of one) is not a sign and is skipped.
int crossings(const std::vector<int16_t>& pcm) {
    int n = 0, last = 0;
    for (int16_t s : pcm) {
        int sign = s > 0 ? 1 : (s < 0 ? -1 : 0);
        if (sign == 0) continue;
        if (last != 0 && sign != last) ++n;
        last = sign;
    }
    return n;
}

// A square wave: the level flips every `half` T-states from `from` up to `until`.
void square(LevelPcm& p, uint64_t from, uint64_t until, uint64_t half, int8_t amp) {
    bool hi = true;
    for (uint64_t t = from; t < until; t += half, hi = !hi) p.edge(t, hi ? amp : (int8_t)-amp);
}

} // namespace

void test_level_pcm() {
    SECTION("LevelPcm: a held level is a constant sample");
    {
        LevelPcm p;
        std::vector<int16_t> pcm;
        p.edge(0, 100);
        p.render(2000000, 2000000, 44100, pcm);  // one second at 2 MHz
        CHECK(pcm.size() == 44100, "one emulated second is one second of samples");
        bool flat = true;
        for (int16_t s : pcm) flat = flat && s == 100 * LevelPcm::kGain;
        CHECK(flat, "every sample is the level times the gain");

        LevelPcm q;
        pcm.clear();
        q.edge(0, -128);
        q.render(2000000, 2000000, 44100, pcm);
        CHECK(!pcm.empty() && pcm.front() == -128 * LevelPcm::kGain,
              "the most negative level is the most negative sample");
        CHECK(q.level() == -128 && !q.pending(), "the level is held and the edge is consumed");
    }

    SECTION("LevelPcm: an edge inside a sample is weighted by where it falls");
    {
        // 441000 Hz and 44100 samples/s: one sample is exactly ten T-states.
        const long long hz = 441000;
        LevelPcm p;
        std::vector<int16_t> pcm;
        p.edge(0, 100);
        p.edge(3, 0);     // 3 T-states at 100, then 7 at 0 -> the average is 30
        p.edge(10, 100);  // a whole sample at 100
        p.edge(25, -100); // the third sample: 5 at 100, 5 at -100 -> 0
        p.render(40, hz, 44100, pcm);
        CHECK(pcm.size() == 4, "forty T-states is four samples");
        CHECK(pcm.size() == 4 && pcm[0] == 30 * LevelPcm::kGain,
              "an edge 30% into a sample gives 30% of the step");
        CHECK(pcm.size() == 4 && pcm[1] == 100 * LevelPcm::kGain, "a whole sample at one level");
        CHECK(pcm.size() == 4 && pcm[2] == 0, "an edge at the middle of a sample averages to zero");
        CHECK(pcm.size() == 4 && pcm[3] == -100 * LevelPcm::kGain, "and the level holds after it");
    }

    SECTION("LevelPcm: a square wave keeps its frequency");
    {
        // 2 MHz, a flip every 1000 T-states: 1000 Hz, 2000 crossings to the second.
        const long long hz = 2000000;
        LevelPcm p;
        std::vector<int16_t> pcm;
        square(p, 0, 2000000, 1000, 64);
        p.render(2000000, hz, 44100, pcm);
        CHECK(pcm.size() == 44100, "one second of samples");
        int n = crossings(pcm);
        CHECK(n >= 1998 && n <= 2000, "1000 Hz crosses zero 2000 times a second");
    }

    SECTION("LevelPcm: many small renders give the same samples as one");
    {
        const long long hz = 2000000;
        const uint64_t  total = 6000000;  // three seconds

        LevelPcm one;
        std::vector<int16_t> whole;
        square(one, 0, total, 1137, 90);  // a half-period that divides nothing evenly
        one.render(total, hz, 44100, whole);

        // The same wave fed and rendered a slice at a time, as a run loop does it, with a
        // slice length that is not a whole number of samples.
        LevelPcm many;
        std::vector<int16_t> pieces;
        uint64_t next = 0;  // the next edge to feed
        bool     hi   = true;
        for (uint64_t t = 0; t < total;) {
            uint64_t end = t + 7919 < total ? t + 7919 : total;
            for (; next < end; next += 1137, hi = !hi) many.edge(next, hi ? 90 : -90);
            many.render(end, hz, 44100, pieces);
            t = end;
        }
        CHECK(whole.size() == 3 * 44100, "three seconds is exactly three seconds of samples");
        CHECK(pieces.size() == whole.size(), "slicing neither adds nor loses a sample");
        CHECK(pieces == whole, "and every sample is the same: no click where slices join");
        CHECK(many.renderedTo() == total, "the rendered point is where the last slice ended");
    }

    SECTION("LevelPcm: time that is skipped makes no samples");
    {
        const long long hz = 2000000;
        LevelPcm p;
        std::vector<int16_t> pcm;
        p.edge(100, 50);
        p.edge(200, -50);
        p.skipTo(1000);
        CHECK(!p.pending() && p.level() == -50, "the skipped edges still set the level");
        p.render(1000 + 2000000, hz, 44100, pcm);
        CHECK(pcm.size() == 44100 && pcm.front() == -50 * LevelPcm::kGain,
              "rendering starts from the skipped-to point, at the level left there");

        // The clock restarts at POWER. A rendered point ahead of the new time must not
        // turn the next render into nothing, or into the whole range of a uint64.
        p.skipTo(0);
        CHECK(p.renderedTo() == 0, "skipping backward moves the rendered point back");
        pcm.clear();
        p.edge(0, 10);
        p.render(2000000, hz, 44100, pcm);
        CHECK(pcm.size() == 44100, "and a second renders as a second again");

        pcm.clear();
        p.render(1000, hz, 44100, pcm);
        CHECK(pcm.empty(), "rendering to a time already rendered makes nothing");

        p.clear();
        CHECK(p.level() == 0 && p.renderedTo() == 0 && !p.pending(), "clear() is 0 V at time zero");
    }

    SECTION("LevelPcm: an edge behind the rendered point takes effect at it");
    {
        const long long hz = 441000;
        LevelPcm p;
        std::vector<int16_t> pcm;
        p.render(100, hz, 44100, pcm);  // ten samples of silence
        pcm.clear();
        p.edge(50, 77);                 // late: it lands at 100
        p.edge(100, 78);                // the same instant: the later write is the level
        p.render(110, hz, 44100, pcm);
        CHECK(pcm.size() == 1 && pcm[0] == 78 * LevelPcm::kGain,
              "two writes at one instant leave the second");
    }
}
