#pragma once

#include <SDL.h>
#include <SDL_ttf.h>

#include <memory>
#include <string>
#include <vector>

#include "Settings.hpp"

namespace cuby {

struct SdlDeleter {
    void operator()(SDL_Window* window) const { SDL_DestroyWindow(window); }
    void operator()(SDL_Renderer* renderer) const { SDL_DestroyRenderer(renderer); }
    void operator()(SDL_Texture* texture) const { SDL_DestroyTexture(texture); }
    void operator()(SDL_Surface* surface) const { SDL_FreeSurface(surface); }
    void operator()(TTF_Font* font) const { TTF_CloseFont(font); }
};

using WindowPtr = std::unique_ptr<SDL_Window, SdlDeleter>;
using RendererPtr = std::unique_ptr<SDL_Renderer, SdlDeleter>;
using TexturePtr = std::unique_ptr<SDL_Texture, SdlDeleter>;
using SurfacePtr = std::unique_ptr<SDL_Surface, SdlDeleter>;
using FontPtr = std::unique_ptr<TTF_Font, SdlDeleter>;

// A rendered piece of text ready to be drawn.
struct Label {
    TexturePtr texture;
    int width = 0;
    int height = 0;

    explicit operator bool() const { return texture != nullptr; }
};

// Owns the SDL library state, the window, the renderer and the UI font.
class Display {
public:
    Display(const std::string& title, const Settings& settings);
    ~Display();

    Display(const Display&) = delete;
    Display& operator=(const Display&) = delete;

    SDL_Renderer* renderer() const { return renderer_.get(); }
    SDL_Window* window() const { return window_.get(); }

    // Size of the drawable area in pixels (not window units, so HiDPI is handled).
    int width() const { return width_; }
    int height() const { return height_; }
    void refreshSize();

    void clear(SDL_Color color);
    void present();

    // Writes the frame drawn since the last clear() to a BMP file. Call it
    // before present(), as the back buffer is undefined afterwards.
    bool saveScreenshot(const std::string& path) const;

    bool fullscreen() const { return fullscreen_; }
    void setFullscreen(bool enabled);

    bool vsync() const { return vsync_; }

    bool hasFont() const { return font_ != nullptr; }
    // Returns an empty label when no font is available.
    Label renderText(const std::string& text, SDL_Color color) const;

    // Paths tried, in order, when looking for the UI font.
    static std::vector<std::string> fontCandidates(const std::string& explicitPath);

private:
    void initialise(const std::string& title, const Settings& settings);
    void shutdown();
    void loadFont(const std::string& explicitPath);

    bool sdlReady_ = false;
    bool ttfReady_ = false;
    WindowPtr window_;
    RendererPtr renderer_;
    FontPtr font_;
    bool fullscreen_ = false;
    bool vsync_ = false;
    int width_ = 0;
    int height_ = 0;
};

}  // namespace cuby
