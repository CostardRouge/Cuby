#include "App.hpp"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

namespace cuby {

namespace {

constexpr const char* kWindowTitle = "Cuby";
constexpr SDL_Color kBackground{8, 8, 14, 255};
constexpr float kMaxFrameTime = 0.1f;    // seconds; clamps dt after a stall
constexpr double kInputGracePeriod = 0.5;  // seconds before --exit-on-input reacts
constexpr float kSpeedStep = 1.15f;

std::string describeSpeed(float speed) {
    std::ostringstream out;
    out << "speed x" << std::fixed << std::setprecision(2) << speed;
    return out.str();
}

}  // namespace

App::App(const Settings& settings)
    : settings_(settings),
      rng_(settings.seed),
      display_(kWindowTitle, settings),
      grid_(settings, rng_),
      overlay_(display_, settings.showFps) {
    grid_.resize(display_.width(), display_.height());
}

int App::run() {
    const Uint64 frequency = SDL_GetPerformanceFrequency();
    Uint64 previous = SDL_GetPerformanceCounter();

    while (running_) {
        const Uint64 frameStart = SDL_GetPerformanceCounter();
        float dt = static_cast<float>(static_cast<double>(frameStart - previous) / static_cast<double>(frequency));
        previous = frameStart;
        dt = std::clamp(dt, 0.0f, kMaxFrameTime);
        elapsed_ += dt;

        handleEvents();
        update(dt);
        render();

        if (settings_.duration > 0.0 && elapsed_ >= settings_.duration) {
            running_ = false;
        }
        limitFrameRate(frameStart);
    }

    if (!settings_.screenshotPath.empty()) {
        // Draw the final state once more without presenting, then read it back.
        display_.clear(kBackground);
        grid_.render(display_.renderer());
        overlay_.render();
        if (display_.saveScreenshot(settings_.screenshotPath)) {
            std::cout << "[Cuby] screenshot saved to " << settings_.screenshotPath << std::endl;
        }
    }
    return 0;
}

void App::handleEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event) != 0) {
        const bool isInput = event.type == SDL_KEYDOWN || event.type == SDL_MOUSEBUTTONDOWN ||
                             event.type == SDL_MOUSEMOTION || event.type == SDL_MOUSEWHEEL;
        if (settings_.exitOnInput && isInput && elapsed_ > kInputGracePeriod) {
            running_ = false;
            return;
        }

        switch (event.type) {
            case SDL_QUIT:
                running_ = false;
                return;
            case SDL_WINDOWEVENT:
                switch (event.window.event) {
                    case SDL_WINDOWEVENT_SIZE_CHANGED:
                        display_.refreshSize();
                        grid_.resize(display_.width(), display_.height());
                        break;
                    case SDL_WINDOWEVENT_ENTER:
                        mouseInside_ = true;
                        break;
                    case SDL_WINDOWEVENT_LEAVE:
                        mouseInside_ = false;
                        break;
                    default:
                        break;
                }
                break;
            case SDL_KEYDOWN:
                if (event.key.repeat == 0) {
                    handleKey(event.key);
                }
                break;
            case SDL_MOUSEMOTION: {
                // Convert window coordinates to drawable pixels (differs on HiDPI displays).
                int windowW = 0;
                int windowH = 0;
                SDL_GetWindowSize(display_.window(), &windowW, &windowH);
                const float scaleX =
                    windowW > 0 ? static_cast<float>(display_.width()) / static_cast<float>(windowW) : 1.0f;
                const float scaleY =
                    windowH > 0 ? static_cast<float>(display_.height()) / static_cast<float>(windowH) : 1.0f;
                mouseX_ = static_cast<int>(static_cast<float>(event.motion.x) * scaleX);
                mouseY_ = static_cast<int>(static_cast<float>(event.motion.y) * scaleY);
                mouseInside_ = true;
                break;
            }
            case SDL_MOUSEWHEEL:
                if (event.wheel.y != 0) {
                    changeSpeed(event.wheel.y > 0 ? kSpeedStep : 1.0f / kSpeedStep);
                }
                break;
            default:
                break;
        }
    }
}

void App::handleKey(const SDL_KeyboardEvent& key) {
    switch (key.keysym.sym) {
        case SDLK_ESCAPE:
        case SDLK_q:
            running_ = false;
            break;
        case SDLK_SPACE:
        case SDLK_p:
            paused_ = !paused_;
            overlay_.notify(paused_ ? "paused" : "resumed");
            break;
        case SDLK_F1:
            overlay_.setShowFps(!overlay_.showFps());
            break;
        case SDLK_F11:
        case SDLK_f:
            display_.setFullscreen(!display_.fullscreen());
            break;
        case SDLK_c: {
            grid_.setColorMode(nextColorMode(grid_.colorMode()));
            std::string text = "color: " + std::string(toString(grid_.colorMode()));
            if (grid_.colorMode() == ColorMode::Groups) {
                text += " (" + std::string(grid_.colorName()) + ")";
            }
            overlay_.notify(text);
            break;
        }
        case SDLK_m:
            grid_.setApparition(nextApparition(grid_.apparition()));
            overlay_.notify("mode: " + std::string(toString(grid_.apparition())));
            break;
        case SDLK_o:
            grid_.setOrigin(nextOrigin(grid_.origin()));
            overlay_.notify("origin: " + std::string(toString(grid_.origin())));
            break;
        case SDLK_s:
            grid_.setShape(nextShape(grid_.shape()));
            overlay_.notify("shape: " + std::string(toString(grid_.shape())));
            break;
        case SDLK_r:
            grid_.restart();
            overlay_.notify("wave restarted");
            break;
        case SDLK_PLUS:
        case SDLK_EQUALS:
        case SDLK_KP_PLUS:
            changeSpeed(kSpeedStep);
            break;
        case SDLK_MINUS:
        case SDLK_KP_MINUS:
            changeSpeed(1.0f / kSpeedStep);
            break;
        default:
            break;
    }
}

void App::changeSpeed(float factor) {
    grid_.setSpeed(grid_.speed() * factor);
    overlay_.notify(describeSpeed(grid_.speed()));
}

void App::update(float dt) {
    if (!paused_) {
        grid_.update(dt, mouseX_, mouseY_, mouseInside_);
    }
    overlay_.update(dt);
}

void App::render() {
    display_.clear(kBackground);
    grid_.render(display_.renderer());
    overlay_.render();
    display_.present();
}

void App::limitFrameRate(Uint64 frameStart) const {
    if (settings_.fpsLimit <= 0) {
        return;
    }
    const Uint64 frequency = SDL_GetPerformanceFrequency();
    const Uint64 target = frequency / static_cast<Uint64>(settings_.fpsLimit);
    const Uint64 spent = SDL_GetPerformanceCounter() - frameStart;
    if (spent >= target) {
        return;
    }
    const Uint64 remainingMs = (target - spent) * 1000 / frequency;
    if (remainingMs >= 1) {
        SDL_Delay(static_cast<Uint32>(remainingMs));
    }
}

}  // namespace cuby
