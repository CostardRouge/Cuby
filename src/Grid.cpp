#include "Grid.hpp"

#include <algorithm>
#include <cmath>

namespace cuby {

namespace {

// Durations in "wave seconds" (real seconds at speed 1.0).
constexpr float kSweepDuration = 4.0f;   // time for the wave front to cross the screen
constexpr float kRevealDuration = 0.9f;  // time for one block to appear
constexpr float kJitter = 0.6f;          // random delay added to each block
constexpr float kHoldDuration = 2.5f;    // pause once every block is visible

constexpr float kHexRatio = 1.1547005f;  // 2 / sqrt(3): hexagon height over width
constexpr float kTopShade = 1.0f;
constexpr float kLeftShade = 0.70f;
constexpr float kRightShade = 0.45f;
constexpr float kHoverStrength = 0.45f;
constexpr float kPi = 3.14159265f;

float easeOutCubic(float t) {
    const float inv = 1.0f - t;
    return 1.0f - inv * inv * inv;
}

// Overshoots slightly past 1 before settling, which gives the "rebound" look.
float easeOutBack(float t) {
    constexpr float c1 = 1.70158f;
    constexpr float c3 = c1 + 1.0f;
    const float inv = t - 1.0f;
    return 1.0f + c3 * inv * inv * inv + c1 * inv * inv;
}

SDL_Color withAlpha(SDL_Color color, float alpha) {
    color.a = static_cast<Uint8>(std::lround(std::clamp(alpha, 0.0f, 1.0f) * 255.0f));
    return color;
}

}  // namespace

Grid::Grid(const Settings& settings, Random& rng) : settings_(settings), rng_(rng), palette_(rng) {}

void Grid::resize(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    layout();
    beginWave();
}

void Grid::layout() {
    blocks_.clear();

    const float size = static_cast<float>(settings_.blockSize);
    if (settings_.shape == Shape::Flat) {
        const int cols = width_ / settings_.blockSize + 1;
        const int rows = height_ / settings_.blockSize + 1;
        blocks_.reserve(static_cast<std::size_t>(cols) * static_cast<std::size_t>(rows));
        for (int row = 0; row < rows; ++row) {
            for (int col = 0; col < cols; ++col) {
                Block block;
                block.cx = static_cast<float>(col) * size + size / 2.0f;
                block.cy = static_cast<float>(row) * size + size / 2.0f;
                blocks_.push_back(block);
            }
        }
    } else {
        // Hexagonal packing: cubes are flush horizontally, rows interleave and
        // every odd row is shifted by half a block ("tumbling blocks" pattern).
        const float hexHeight = size * kHexRatio;
        const float rowPitch = hexHeight * 0.75f;
        const int cols = width_ / settings_.blockSize + 2;
        const int rows = static_cast<int>(static_cast<float>(height_) / rowPitch) + 2;
        blocks_.reserve(static_cast<std::size_t>(cols) * static_cast<std::size_t>(rows));
        for (int row = 0; row < rows; ++row) {
            const float shift = (row % 2 == 1) ? size / 2.0f : 0.0f;
            for (int col = 0; col < cols; ++col) {
                Block block;
                block.cx = static_cast<float>(col) * size + shift;
                block.cy = static_cast<float>(row) * rowPitch;
                blocks_.push_back(block);
            }
        }
    }

    vertices_.reserve(blocks_.size() * 36);
}

float Grid::normalizedDistance(const Block& block) const {
    const float w = static_cast<float>(width_);
    const float h = static_cast<float>(height_);
    float distance = 0.0f;
    switch (settings_.origin) {
        case Origin::Top:
            distance = block.cy / h;
            break;
        case Origin::Bottom:
            distance = 1.0f - block.cy / h;
            break;
        case Origin::Left:
            distance = block.cx / w;
            break;
        case Origin::Right:
            distance = 1.0f - block.cx / w;
            break;
        case Origin::Center: {
            const float dx = block.cx - w / 2.0f;
            const float dy = block.cy - h / 2.0f;
            const float halfDiagonal = 0.5f * std::sqrt(w * w + h * h);
            distance = std::sqrt(dx * dx + dy * dy) / halfDiagonal;
            break;
        }
        case Origin::Random:
            distance = 0.0f;  // the jitter below spreads the blocks over the sweep
            break;
    }
    return std::clamp(distance, 0.0f, 1.0f);
}

void Grid::beginWave() {
    ++waveCount_;
    palette_.beginWave(settings_.colorMode, settings_.colorGroup);

    const float revealDuration = settings_.apparition == Apparition::Instant ? 0.0f : kRevealDuration;
    const float jitter = settings_.origin == Origin::Random ? kSweepDuration : kJitter;
    const float w = static_cast<float>(width_);
    const float h = static_cast<float>(height_);
    float lastStart = 0.0f;

    for (Block& block : blocks_) {
        // Whatever is on screen right now becomes the base for the next reveal.
        if (block.progress > 0.0f) {
            SDL_Color from = block.current.a > 0 ? block.current
                                                 : SDL_Color{block.target.r, block.target.g, block.target.b, 0};
            block.current = mix(from, block.target, block.progress);
        }
        block.target = palette_.colorAt(block.cx / w, block.cy / h);
        block.start = normalizedDistance(block) * kSweepDuration + rng_.range(0.0f, jitter);
        block.progress = 0.0f;
        lastStart = std::max(lastStart, block.start);

        switch (settings_.origin) {
            case Origin::Top:
                block.dirX = 0.0f;
                block.dirY = 1.0f;
                break;
            case Origin::Bottom:
                block.dirX = 0.0f;
                block.dirY = -1.0f;
                break;
            case Origin::Left:
                block.dirX = 1.0f;
                block.dirY = 0.0f;
                break;
            case Origin::Right:
                block.dirX = -1.0f;
                block.dirY = 0.0f;
                break;
            case Origin::Center: {
                const float dx = block.cx - w / 2.0f;
                const float dy = block.cy - h / 2.0f;
                const float length = std::sqrt(dx * dx + dy * dy);
                block.dirX = length > 0.001f ? dx / length : 0.0f;
                block.dirY = length > 0.001f ? dy / length : 1.0f;
                break;
            }
            case Origin::Random: {
                const float angle = rng_.range(0.0f, 2.0f * kPi);
                block.dirX = std::cos(angle);
                block.dirY = std::sin(angle);
                break;
            }
        }
    }

    waveTime_ = 0.0f;
    waveEnd_ = lastStart + revealDuration + kHoldDuration;
}

void Grid::restart() { beginWave(); }

void Grid::setColorMode(ColorMode mode) {
    settings_.colorMode = mode;
    beginWave();
}

void Grid::setOrigin(Origin origin) {
    settings_.origin = origin;
    beginWave();
}

void Grid::setApparition(Apparition apparition) { settings_.apparition = apparition; }

void Grid::setShape(Shape shape) {
    if (shape == settings_.shape) {
        return;
    }
    settings_.shape = shape;
    layout();
    beginWave();
}

void Grid::setSpeed(float speed) { settings_.speed = std::clamp(speed, kMinSpeed, kMaxSpeed); }

void Grid::update(float dt, int mouseX, int mouseY, bool mouseInside) {
    waveTime_ += dt * settings_.speed;

    const float revealDuration = settings_.apparition == Apparition::Instant ? 0.0f : kRevealDuration;
    const float half = static_cast<float>(settings_.blockSize) / 2.0f;
    const float halfHeight = settings_.shape == Shape::Cube ? half * kHexRatio : half;
    const float mx = static_cast<float>(mouseX);
    const float my = static_cast<float>(mouseY);

    for (Block& block : blocks_) {
        if (block.progress < 1.0f) {
            if (revealDuration <= 0.0f) {
                block.progress = waveTime_ >= block.start ? 1.0f : 0.0f;
            } else {
                block.progress = std::clamp((waveTime_ - block.start) / revealDuration, 0.0f, 1.0f);
            }
            if (block.progress >= 1.0f) {
                block.current = block.target;
            }
        }

        bool hovered = false;
        if (mouseInside) {
            const float dx = std::fabs(mx - block.cx);
            const float dy = std::fabs(my - block.cy);
            if (settings_.shape == Shape::Cube) {
                // Point-in-hexagon test for a pointy-top hexagon.
                hovered = dx <= half && dy <= halfHeight - (dx / half) * (halfHeight / 2.0f);
            } else {
                hovered = dx <= half && dy <= halfHeight;
            }
        }
        const float hoverTarget = hovered ? 1.0f : 0.0f;
        const float hoverRate = hovered ? 12.0f : 2.5f;
        block.hover += (hoverTarget - block.hover) * std::min(1.0f, hoverRate * dt);
    }

    if (waveTime_ >= waveEnd_) {
        beginWave();
    }
}

void Grid::pushQuad(float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3,
                    SDL_Color color) {
    const SDL_FPoint uv{0.0f, 0.0f};
    const SDL_Vertex v0{{x0, y0}, color, uv};
    const SDL_Vertex v1{{x1, y1}, color, uv};
    const SDL_Vertex v2{{x2, y2}, color, uv};
    const SDL_Vertex v3{{x3, y3}, color, uv};
    vertices_.push_back(v0);
    vertices_.push_back(v1);
    vertices_.push_back(v2);
    vertices_.push_back(v0);
    vertices_.push_back(v2);
    vertices_.push_back(v3);
}

void Grid::pushBlock(float cx, float cy, float scale, SDL_Color color) {
    if (scale <= 0.0f || color.a == 0) {
        return;
    }
    const float half = static_cast<float>(settings_.blockSize - settings_.gap) / 2.0f * scale;

    if (settings_.shape == Shape::Flat) {
        pushQuad(cx - half, cy - half, cx + half, cy - half, cx + half, cy + half, cx - half, cy + half,
                 color);
        return;
    }

    const float hw = half;
    const float hh = half * kHexRatio;
    const float top = cy - hh;
    const float upper = cy - hh / 2.0f;
    const float lower = cy + hh / 2.0f;
    const float bottom = cy + hh;
    const float left = cx - hw;
    const float right = cx + hw;

    // Top face (rhombus), then the two visible side faces, lit from the top-left.
    pushQuad(cx, top, right, upper, cx, cy, left, upper, shade(color, kTopShade));
    pushQuad(left, upper, cx, cy, cx, bottom, left, lower, shade(color, kLeftShade));
    pushQuad(cx, cy, right, upper, right, lower, cx, bottom, shade(color, kRightShade));
}

void Grid::render(SDL_Renderer* renderer) {
    vertices_.clear();

    const SDL_Color white{255, 255, 255, 255};
    const float slideDistance = static_cast<float>(settings_.blockSize) * 1.5f;

    for (const Block& block : blocks_) {
        const bool revealing = block.progress > 0.0f && block.progress < 1.0f;

        if (block.current.a > 0) {
            SDL_Color base = mix(block.current, white, block.hover * kHoverStrength);
            base.a = block.current.a;
            pushBlock(block.cx, block.cy, 1.0f, base);
        }
        if (!revealing) {
            continue;
        }

        SDL_Color color = mix(block.target, white, block.hover * kHoverStrength);
        float scale = 1.0f;
        float offsetX = 0.0f;
        float offsetY = 0.0f;
        switch (settings_.apparition) {
            case Apparition::Fade:
                color = withAlpha(color, block.progress);
                break;
            case Apparition::Rebound:
                scale = std::max(0.0f, easeOutBack(block.progress));
                color = withAlpha(color, std::min(1.0f, block.progress * 4.0f));
                break;
            case Apparition::Slide: {
                const float remaining = (1.0f - easeOutCubic(block.progress)) * slideDistance;
                offsetX = -block.dirX * remaining;
                offsetY = -block.dirY * remaining;
                color = withAlpha(color, std::min(1.0f, block.progress * 2.5f));
                break;
            }
            case Apparition::Instant:
                color = withAlpha(color, 1.0f);
                break;
        }
        pushBlock(block.cx + offsetX, block.cy + offsetY, scale, color);
    }

    if (!vertices_.empty()) {
        SDL_RenderGeometry(renderer, nullptr, vertices_.data(), static_cast<int>(vertices_.size()), nullptr,
                           0);
    }
}

}  // namespace cuby
