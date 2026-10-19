#include "Palette.hpp"

#include <algorithm>
#include <cmath>

namespace cuby {

namespace {

Uint8 toByte(float value) {
    return static_cast<Uint8>(std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f));
}

}  // namespace

SDL_Color hsvToRgb(float hue, float saturation, float value, Uint8 alpha) {
    hue = std::fmod(hue, 360.0f);
    if (hue < 0.0f) {
        hue += 360.0f;
    }
    saturation = std::clamp(saturation, 0.0f, 1.0f);
    value = std::clamp(value, 0.0f, 1.0f);

    const float c = value * saturation;
    const float hPrime = hue / 60.0f;
    const float x = c * (1.0f - std::fabs(std::fmod(hPrime, 2.0f) - 1.0f));
    const float m = value - c;

    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    if (hPrime < 1.0f) {
        r = c;
        g = x;
    } else if (hPrime < 2.0f) {
        r = x;
        g = c;
    } else if (hPrime < 3.0f) {
        g = c;
        b = x;
    } else if (hPrime < 4.0f) {
        g = x;
        b = c;
    } else if (hPrime < 5.0f) {
        r = x;
        b = c;
    } else {
        r = c;
        b = x;
    }
    return SDL_Color{toByte(r + m), toByte(g + m), toByte(b + m), alpha};
}

SDL_Color mix(SDL_Color from, SDL_Color to, float amount) {
    amount = std::clamp(amount, 0.0f, 1.0f);
    auto channel = [amount](Uint8 a, Uint8 b) {
        return static_cast<Uint8>(std::lround(a + (static_cast<float>(b) - a) * amount));
    };
    return SDL_Color{channel(from.r, to.r), channel(from.g, to.g), channel(from.b, to.b),
                     channel(from.a, to.a)};
}

SDL_Color shade(SDL_Color color, float factor) {
    auto channel = [factor](Uint8 value) {
        return static_cast<Uint8>(std::lround(std::clamp(value * factor, 0.0f, 255.0f)));
    };
    return SDL_Color{channel(color.r), channel(color.g), channel(color.b), color.a};
}

Palette::Palette(Random& rng) : rng_(rng) {}

void Palette::beginWave(ColorMode mode, int fixedGroup) {
    mode_ = mode;
    ++wave_;

    const int groupCount = static_cast<int>(colorGroups().size());
    switch (mode_) {
        case ColorMode::Fixed:
            group_ = std::clamp(fixedGroup, 0, groupCount - 1);
            break;
        case ColorMode::Groups: {
            // Pick a family that differs from the previous wave so the change is visible.
            int next = rng_.between(0, groupCount - 1);
            if (next == group_ && groupCount > 1) {
                next = (next + 1 + rng_.between(0, groupCount - 2)) % groupCount;
            }
            group_ = next;
            break;
        }
        case ColorMode::Rainbow:
            hueOffset_ = rng_.range(0.0f, 360.0f);
            break;
        case ColorMode::Random:
        case ColorMode::Mono:
            break;
    }
}

SDL_Color Palette::colorAt(float u, float v) {
    u = std::clamp(u, 0.0f, 1.0f);
    v = std::clamp(v, 0.0f, 1.0f);

    switch (mode_) {
        case ColorMode::Rainbow: {
            const float hue = hueOffset_ + (u * 0.7f + v * 0.3f) * 360.0f;
            const float value = 0.72f + 0.28f * rng_.unit();
            return hsvToRgb(hue, 0.85f, value);
        }
        case ColorMode::Groups:
        case ColorMode::Fixed: {
            const ColorGroup& group = colorGroups()[static_cast<std::size_t>(std::max(group_, 0))];
            const float hue = group.hue + rng_.range(-8.0f, 8.0f);
            // Bias towards dark tones so the bright blocks stand out, like the original demo.
            const float value = 0.12f + 0.88f * std::pow(rng_.unit(), 1.6f);
            return hsvToRgb(hue, group.saturation, value);
        }
        case ColorMode::Random:
            return hsvToRgb(rng_.range(0.0f, 360.0f), rng_.range(0.55f, 1.0f), rng_.range(0.45f, 1.0f));
        case ColorMode::Mono: {
            const float value = 0.08f + 0.92f * std::pow(rng_.unit(), 1.4f);
            return hsvToRgb(0.0f, 0.0f, value);
        }
    }
    return SDL_Color{255, 0, 255, 255};
}

std::string_view Palette::groupName() const {
    return colorGroups()[static_cast<std::size_t>(std::max(group_, 0))].name;
}

}  // namespace cuby
