#include "Overlay.hpp"

#include <cmath>
#include <sstream>

namespace cuby {

namespace {

constexpr float kFpsRefreshInterval = 0.5f;  // seconds
constexpr float kMessageDuration = 2.0f;     // seconds
constexpr int kMargin = 10;
constexpr int kPadding = 6;
constexpr SDL_Color kTextColor{255, 255, 255, 255};
constexpr SDL_Color kBoxColor{0, 0, 0, 150};

void drawLabel(SDL_Renderer* renderer, const Label& label, int x, int y, Uint8 alpha) {
    if (!label) {
        return;
    }
    const SDL_Rect box{x - kPadding, y - kPadding, label.width + 2 * kPadding, label.height + 2 * kPadding};
    SDL_SetRenderDrawColor(renderer, kBoxColor.r, kBoxColor.g, kBoxColor.b,
                           static_cast<Uint8>(kBoxColor.a * alpha / 255));
    SDL_RenderFillRect(renderer, &box);

    const SDL_Rect destination{x, y, label.width, label.height};
    SDL_SetTextureAlphaMod(label.texture.get(), alpha);
    SDL_RenderCopy(renderer, label.texture.get(), nullptr, &destination);
}

}  // namespace

Overlay::Overlay(Display& display, bool showFps) : display_(display), showFps_(showFps) {}

void Overlay::notify(const std::string& message) {
    messageLabel_ = display_.renderText(message, kTextColor);
    messageTimeLeft_ = messageLabel_ ? kMessageDuration : 0.0f;
}

void Overlay::update(float dt) {
    ++frames_;
    fpsAccumulator_ += dt;
    if (fpsAccumulator_ >= kFpsRefreshInterval) {
        fps_ = static_cast<float>(frames_) / fpsAccumulator_;
        frames_ = 0;
        fpsAccumulator_ = 0.0f;
        if (showFps_) {
            std::ostringstream text;
            text << static_cast<int>(std::lround(fps_)) << " FPS";
            fpsLabel_ = display_.renderText(text.str(), kTextColor);
        }
    }

    if (messageTimeLeft_ > 0.0f) {
        messageTimeLeft_ -= dt;
        if (messageTimeLeft_ <= 0.0f) {
            messageLabel_ = Label{};
        }
    }
}

void Overlay::render() {
    SDL_Renderer* renderer = display_.renderer();
    if (showFps_) {
        drawLabel(renderer, fpsLabel_, kMargin + kPadding, kMargin + kPadding, 255);
    }
    if (messageLabel_ && messageTimeLeft_ > 0.0f) {
        // Fade the notification out during its last half second.
        const float alpha = std::min(1.0f, messageTimeLeft_ * 2.0f);
        const int y = display_.height() - kMargin - kPadding - messageLabel_.height;
        drawLabel(renderer, messageLabel_, kMargin + kPadding, y, static_cast<Uint8>(alpha * 255.0f));
    }
}

}  // namespace cuby
