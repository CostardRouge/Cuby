#pragma once

#include <SDL.h>

#include <vector>

#include "Palette.hpp"
#include "Random.hpp"
#include "Settings.hpp"

namespace cuby {

// One cell of the grid. Positions are pixel coordinates of the block center.
struct Block {
    float cx = 0.0f;
    float cy = 0.0f;
    SDL_Color current{0, 0, 0, 0};  // color left by the previous wave (alpha 0 = nothing yet)
    SDL_Color target{0, 0, 0, 0};   // color revealed by the current wave
    float start = 0.0f;             // wave time at which the reveal starts
    float progress = 0.0f;          // reveal progress, 0..1
    float dirX = 0.0f;              // slide direction (unit vector)
    float dirY = 1.0f;
    float hover = 0.0f;  // mouse highlight, 0..1
};

// The field of blocks and the wave animation that reveals them.
class Grid {
public:
    Grid(const Settings& settings, Random& rng);

    // Lays the blocks out for a drawable area of the given size and restarts the wave.
    void resize(int width, int height);

    // Advances the animation. `dt` is in seconds; the mouse position is in pixels.
    void update(float dt, int mouseX, int mouseY, bool mouseInside);

    // Draws every block with one batched geometry call.
    void render(SDL_Renderer* renderer);

    // Starts a new wave immediately.
    void restart();

    ColorMode colorMode() const { return settings_.colorMode; }
    void setColorMode(ColorMode mode);
    Origin origin() const { return settings_.origin; }
    void setOrigin(Origin origin);
    Apparition apparition() const { return settings_.apparition; }
    void setApparition(Apparition apparition);
    Shape shape() const { return settings_.shape; }
    void setShape(Shape shape);

    float speed() const { return settings_.speed; }
    void setSpeed(float speed);

    int blockCount() const { return static_cast<int>(blocks_.size()); }
    int waveCount() const { return waveCount_; }
    std::string_view colorName() const { return palette_.groupName(); }

private:
    void layout();
    void beginWave();
    float normalizedDistance(const Block& block) const;
    void pushQuad(float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3,
                  SDL_Color color);
    void pushBlock(float cx, float cy, float scale, SDL_Color color);

    Settings settings_;
    Random& rng_;
    Palette palette_;

    int width_ = 1;
    int height_ = 1;
    std::vector<Block> blocks_;
    std::vector<SDL_Vertex> vertices_;  // scratch buffer reused every frame

    float waveTime_ = 0.0f;  // seconds, scaled by the speed multiplier
    float waveEnd_ = 0.0f;   // wave time at which the next wave starts
    int waveCount_ = 0;
};

}  // namespace cuby
