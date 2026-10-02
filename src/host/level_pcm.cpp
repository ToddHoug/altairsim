#include "host/level_pcm.h"

#include <cstddef>

namespace altair {

void LevelPcm::edge(uint64_t t, int8_t level) {
    if (t < t_) t = t_;
    // Two writes at one instant: the later one is the level. Keeps the list in strict
    // time order, and a guest that writes the same port twice in a row costs nothing.
    if (!edges_.empty() && edges_.back().t >= t) {
        edges_.back().level = level;
        return;
    }
    edges_.push_back({t, level});
}

void LevelPcm::hold(uint64_t dt, std::vector<int16_t>& out) {
    const uint64_t hz    = (uint64_t)hz_;
    uint64_t       units = dt * (uint64_t)rate_;

    // Finish the sample in progress.
    if (phase_ != 0) {
        uint64_t take = hz - phase_;
        if (take > units) take = units;
        sum_   += (int64_t)level_ * (int64_t)take;
        phase_ += take;
        units  -= take;
        if (phase_ < hz) return;
        out.push_back((int16_t)(sum_ * kGain / (int64_t)hz));
        phase_ = 0;
        sum_   = 0;
    }

    // Whole samples at one level need no arithmetic.
    const uint64_t whole = units / hz;
    out.insert(out.end(), (size_t)whole, (int16_t)(level_ * kGain));
    units -= whole * hz;

    // Start the next one.
    phase_ = units;
    sum_   = (int64_t)level_ * (int64_t)units;
}

void LevelPcm::render(uint64_t untilT, long long hz, int rate, std::vector<int16_t>& out) {
    if (hz <= 0 || rate <= 0 || untilT <= t_) return;

    // A different crystal or device rate changes the size of a unit, so the part of a
    // sample counted in the old units means nothing now. Drop it: one sample, once.
    if (hz != hz_ || rate != rate_) {
        hz_    = hz;
        rate_  = rate;
        phase_ = 0;
        sum_   = 0;
    }

    size_t used = 0;
    for (; used < edges_.size() && edges_[used].t <= untilT; ++used) {
        hold(edges_[used].t - t_, out);
        t_     = edges_[used].t;
        level_ = edges_[used].level;
    }
    edges_.erase(edges_.begin(), edges_.begin() + (std::ptrdiff_t)used);

    hold(untilT - t_, out);
    t_ = untilT;
}

void LevelPcm::skipTo(uint64_t t) {
    size_t used = 0;
    for (; used < edges_.size() && edges_[used].t <= t; ++used) level_ = edges_[used].level;
    edges_.erase(edges_.begin(), edges_.begin() + (std::ptrdiff_t)used);
    // An edge still ahead of a clock that went BACK belongs to a time that will not come.
    if (t < t_) {
        if (!edges_.empty()) level_ = edges_.back().level;
        edges_.clear();
    }
    t_     = t;
    phase_ = 0;
    sum_   = 0;
}

void LevelPcm::clear() {
    edges_.clear();
    t_     = 0;
    level_ = 0;
    phase_ = 0;
    sum_   = 0;
}

} // namespace altair
