#pragma once

#include <SDL.h>

#include "Display.hpp"
#include "Grid.hpp"
#include "Overlay.hpp"
#include "Random.hpp"
#include "Settings.hpp"

namespace cuby {

// Owns everything and runs the main loop.
class App {
public:
    explicit App(const Settings& settings);

    // Runs until the window is closed, a quit key is pressed or the duration
    // elapses. Returns the process exit code.
    int run();

private:
    void handleEvents();
    void handleKey(const SDL_KeyboardEvent& key);
    void changeSpeed(float factor);
    void update(float dt);
    void render();
    void limitFrameRate(Uint64 frameStart) const;

    Settings settings_;
    Random rng_;
    Display display_;
    Grid grid_;
    Overlay overlay_;

    bool running_ = true;
    bool paused_ = false;
    bool mouseInside_ = false;
    int mouseX_ = 0;
    int mouseY_ = 0;
    double elapsed_ = 0.0;  // real seconds since start
};

}  // namespace cuby
