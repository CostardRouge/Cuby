#pragma once

#include <SDL.h>

#include <string_view>

#include "Random.hpp"
#include "Settings.hpp"

namespace cuby {

// Converts HSV (hue in degrees, saturation and value in 0..1) to an SDL color.
SDL_Color hsvToRgb(float hue, float saturation, float value, Uint8 alpha = 255);

// Mixes two colors; amount 0 gives `from`, amount 1 gives `to`.
SDL_Color mix(SDL_Color from, SDL_Color to, float amount);

// Scales the RGB channels of a color (used for cube face shading).
SDL_Color shade(SDL_Color color, float factor);

// Chooses block colors for each wave according to the active color mode.
class Palette {
public:
    explicit Palette(Random& rng);

    // Starts a new wave: picks the wave-level parameters (color family, hue rotation).
    void beginWave(ColorMode mode, int fixedGroup);

    // Color of a block at the normalized screen position (u, v) in [0, 1].
    SDL_Color colorAt(float u, float v);

    // Name of the color family used by the current wave (Groups/Fixed modes).
    std::string_view groupName() const;

private:
    Random& rng_;
    ColorMode mode_ = ColorMode::Groups;
    int group_ = -1;  // -1 until the first wave picks a family
    float hueOffset_ = 0.0f;
    int wave_ = 0;
};

}  // namespace cuby
