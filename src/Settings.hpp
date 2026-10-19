#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cuby {

// How block colors are chosen for each wave.
enum class ColorMode {
    Rainbow,  // hue follows the block position
    Groups,   // one color family per wave (red, blue, ...)
    Random,   // every block gets an independent random color
    Mono,     // grayscale
    Fixed,    // one color family chosen on the command line
};

// Where a wave of blocks starts from.
enum class Origin { Top, Bottom, Left, Right, Center, Random };

// How a block appears when the wave reaches it.
enum class Apparition { Fade, Rebound, Slide, Instant };

// What a block looks like.
enum class Shape { Cube, Flat };

std::string_view toString(ColorMode mode);
std::string_view toString(Origin origin);
std::string_view toString(Apparition apparition);
std::string_view toString(Shape shape);

std::optional<ColorMode> parseColorMode(std::string_view text);
std::optional<Origin> parseOrigin(std::string_view text);
std::optional<Apparition> parseApparition(std::string_view text);
std::optional<Shape> parseShape(std::string_view text);

// Cycle helpers used by the runtime key bindings.
ColorMode nextColorMode(ColorMode mode);
Origin nextOrigin(Origin origin);
Apparition nextApparition(Apparition apparition);
Shape nextShape(Shape shape);

// Named color families available for `--color <name>` and the Groups mode.
struct ColorGroup {
    std::string_view name;
    float hue;         // degrees, 0..360
    float saturation;  // 0..1
};

const std::vector<ColorGroup>& colorGroups();
std::optional<int> findColorGroup(std::string_view name);

struct Settings {
    int width = 1024;
    int height = 600;
    bool fullscreen = false;

    int blockSize = 40;  // pixels
    int gap = 0;         // pixels of spacing between blocks
    float speed = 1.0f;  // animation speed multiplier

    ColorMode colorMode = ColorMode::Groups;
    int colorGroup = 0;  // index into colorGroups(), used by ColorMode::Fixed
    Origin origin = Origin::Random;
    Apparition apparition = Apparition::Fade;
    Shape shape = Shape::Cube;

    int fpsLimit = 60;  // 0 = unlimited
    bool vsync = true;
    bool showFps = false;
    bool hideCursor = false;
    bool exitOnInput = false;  // behave like a real screensaver
    double duration = 0.0;     // seconds, 0 = run until closed

    std::optional<std::uint32_t> seed;
    std::string fontPath;        // empty = search the default locations
    std::string screenshotPath;  // when set, the last frame is saved there as a BMP
};

// Limits applied to the numeric options.
constexpr int kMinBlockSize = 4;
constexpr int kMaxBlockSize = 1024;
constexpr float kMinSpeed = 0.05f;
constexpr float kMaxSpeed = 20.0f;
constexpr int kMinWindowSize = 64;
constexpr int kMaxWindowSize = 16384;

struct ParseOutcome {
    enum class Action { Run, Exit };

    Action action = Action::Run;
    int exitCode = 0;
    std::string message;  // help, version or error text to print before exiting
    Settings settings;
};

// Parses the command line (without the program name). Never throws.
ParseOutcome parseArguments(const std::vector<std::string_view>& args);

std::string usageText();
std::string versionText();

}  // namespace cuby
