#include "Display.hpp"

#include <filesystem>
#include <iostream>

#include "Exception.hpp"

#ifndef CUBY_SOURCE_ASSET_DIR
#define CUBY_SOURCE_ASSET_DIR ""
#endif
#ifndef CUBY_INSTALL_ASSET_DIR
#define CUBY_INSTALL_ASSET_DIR ""
#endif

namespace cuby {

namespace {

constexpr int kFontPointSize = 18;
constexpr const char* kFontFileName = "font.ttf";

std::string sdlError(const std::string& context) {
    const char* error = SDL_GetError();
    return context + ": " + ((error != nullptr && *error != '\0') ? error : "unknown SDL error");
}

}  // namespace

Display::Display(const std::string& title, const Settings& settings) {
    try {
        initialise(title, settings);
    } catch (...) {
        // The destructor does not run when the constructor throws, so release
        // what was acquired before propagating the error.
        shutdown();
        throw;
    }
}

Display::~Display() { shutdown(); }

void Display::initialise(const std::string& title, const Settings& settings) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        throw Exception(sdlError("SDL_Init failed"));
    }
    sdlReady_ = true;

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

    Uint32 windowFlags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
    if (settings.fullscreen) {
        windowFlags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    }
    window_.reset(SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                   settings.width, settings.height, windowFlags));
    if (!window_) {
        throw Exception(sdlError("SDL_CreateWindow failed"));
    }
    fullscreen_ = settings.fullscreen;

    Uint32 rendererFlags = SDL_RENDERER_ACCELERATED;
    if (settings.vsync) {
        rendererFlags |= SDL_RENDERER_PRESENTVSYNC;
    }
    renderer_.reset(SDL_CreateRenderer(window_.get(), -1, rendererFlags));
    if (!renderer_ && settings.vsync) {
        // Some drivers (and the headless "dummy" driver) refuse vsync: retry without it.
        renderer_.reset(SDL_CreateRenderer(window_.get(), -1, SDL_RENDERER_ACCELERATED));
    }
    if (!renderer_) {
        renderer_.reset(SDL_CreateRenderer(window_.get(), -1, 0));
    }
    if (!renderer_) {
        throw Exception(sdlError("SDL_CreateRenderer failed"));
    }

    SDL_RendererInfo info{};
    if (SDL_GetRendererInfo(renderer_.get(), &info) == 0) {
        vsync_ = (info.flags & SDL_RENDERER_PRESENTVSYNC) != 0;
    }
    SDL_SetRenderDrawBlendMode(renderer_.get(), SDL_BLENDMODE_BLEND);
    refreshSize();

    if (settings.hideCursor) {
        SDL_ShowCursor(SDL_DISABLE);
    }

    if (TTF_Init() != 0) {
        std::cerr << "[Cuby] warning: SDL_ttf failed to initialise (" << TTF_GetError()
                  << "); on-screen text is disabled" << std::endl;
    } else {
        ttfReady_ = true;
        loadFont(settings.fontPath);
    }
}

void Display::shutdown() {
    font_.reset();
    renderer_.reset();
    window_.reset();
    if (ttfReady_) {
        TTF_Quit();
        ttfReady_ = false;
    }
    if (sdlReady_) {
        SDL_Quit();
        sdlReady_ = false;
    }
}

std::vector<std::string> Display::fontCandidates(const std::string& explicitPath) {
    std::vector<std::string> candidates;
    if (!explicitPath.empty()) {
        candidates.push_back(explicitPath);
        return candidates;
    }

    if (char* basePath = SDL_GetBasePath(); basePath != nullptr) {
        const std::string base(basePath);
        SDL_free(basePath);
        candidates.push_back(base + "Assets/" + kFontFileName);
        candidates.push_back(base + "../Assets/" + kFontFileName);
        candidates.push_back(base + "../share/cuby/" + kFontFileName);
    }
    candidates.push_back(std::string("Assets/") + kFontFileName);
    if (const std::string sourceDir = CUBY_SOURCE_ASSET_DIR; !sourceDir.empty()) {
        candidates.push_back(sourceDir + "/" + kFontFileName);
    }
    if (const std::string installDir = CUBY_INSTALL_ASSET_DIR; !installDir.empty()) {
        candidates.push_back(installDir + "/" + kFontFileName);
    }
    return candidates;
}

void Display::loadFont(const std::string& explicitPath) {
    std::error_code ec;
    for (const std::string& candidate : fontCandidates(explicitPath)) {
        if (!std::filesystem::is_regular_file(candidate, ec)) {
            continue;
        }
        font_.reset(TTF_OpenFont(candidate.c_str(), kFontPointSize));
        if (font_) {
            return;
        }
        std::cerr << "[Cuby] warning: cannot open font " << candidate << " (" << TTF_GetError() << ")"
                  << std::endl;
    }
    std::cerr << "[Cuby] warning: no usable font found; on-screen text is disabled"
              << " (use --font <path>)" << std::endl;
}

void Display::refreshSize() {
    int w = 0;
    int h = 0;
    if (SDL_GetRendererOutputSize(renderer_.get(), &w, &h) != 0) {
        SDL_GetWindowSize(window_.get(), &w, &h);
    }
    width_ = w > 0 ? w : 1;
    height_ = h > 0 ? h : 1;
}

void Display::clear(SDL_Color color) {
    SDL_SetRenderDrawColor(renderer_.get(), color.r, color.g, color.b, color.a);
    SDL_RenderClear(renderer_.get());
}

void Display::present() { SDL_RenderPresent(renderer_.get()); }

bool Display::saveScreenshot(const std::string& path) const {
    SurfacePtr surface(SDL_CreateRGBSurfaceWithFormat(0, width_, height_, 32, SDL_PIXELFORMAT_ARGB8888));
    if (!surface) {
        std::cerr << "[Cuby] warning: " << sdlError("cannot allocate the screenshot surface") << std::endl;
        return false;
    }
    if (SDL_RenderReadPixels(renderer_.get(), nullptr, surface->format->format, surface->pixels,
                             surface->pitch) != 0) {
        std::cerr << "[Cuby] warning: " << sdlError("cannot read the frame back") << std::endl;
        return false;
    }
    if (SDL_SaveBMP(surface.get(), path.c_str()) != 0) {
        std::cerr << "[Cuby] warning: " << sdlError("cannot write " + path) << std::endl;
        return false;
    }
    return true;
}

void Display::setFullscreen(bool enabled) {
    if (SDL_SetWindowFullscreen(window_.get(), enabled ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0) != 0) {
        std::cerr << "[Cuby] warning: " << sdlError("cannot change fullscreen state") << std::endl;
        return;
    }
    fullscreen_ = enabled;
    refreshSize();
}

Label Display::renderText(const std::string& text, SDL_Color color) const {
    Label label;
    if (!font_ || text.empty()) {
        return label;
    }
    SurfacePtr surface(TTF_RenderUTF8_Blended(font_.get(), text.c_str(), color));
    if (!surface) {
        return label;
    }
    label.texture.reset(SDL_CreateTextureFromSurface(renderer_.get(), surface.get()));
    if (!label.texture) {
        return label;
    }
    label.width = surface->w;
    label.height = surface->h;
    return label;
}

}  // namespace cuby
