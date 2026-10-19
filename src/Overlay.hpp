#pragma once

#include <string>

#include "Display.hpp"

namespace cuby {

// On-screen text: the frame counter and short notifications about key presses.
class Overlay {
public:
    explicit Overlay(Display& display, bool showFps);

    bool showFps() const { return showFps_; }
    void setShowFps(bool enabled) { showFps_ = enabled; }

    // Shows a message in the bottom-left corner for a couple of seconds.
    void notify(const std::string& message);

    // `dt` is real elapsed seconds (not scaled by the animation speed).
    void update(float dt);
    void render();

    float fps() const { return fps_; }

private:
    Display& display_;
    bool showFps_ = false;

    float fpsAccumulator_ = 0.0f;
    int frames_ = 0;
    float fps_ = 0.0f;
    Label fpsLabel_;

    Label messageLabel_;
    float messageTimeLeft_ = 0.0f;
};

}  // namespace cuby
