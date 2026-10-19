#pragma once

#include <cstdint>
#include <optional>
#include <random>
#include <utility>

namespace cuby {

// Small wrapper around std::mt19937 so every random draw in the program goes
// through one seeded generator. Passing a seed makes a run reproducible.
class Random {
public:
    explicit Random(std::optional<std::uint32_t> seed = std::nullopt)
        : seed_(seed ? *seed : std::random_device{}()), engine_(seed_) {}

    std::uint32_t seed() const { return seed_; }

    // Uniform integer in [lo, hi] (both inclusive).
    int between(int lo, int hi) {
        if (lo > hi) {
            std::swap(lo, hi);
        }
        return std::uniform_int_distribution<int>(lo, hi)(engine_);
    }

    // Uniform float in [0, 1).
    float unit() { return std::uniform_real_distribution<float>(0.0f, 1.0f)(engine_); }

    // Uniform float in [lo, hi).
    float range(float lo, float hi) { return lo + (hi - lo) * unit(); }

private:
    std::uint32_t seed_;
    std::mt19937 engine_;
};

}  // namespace cuby
