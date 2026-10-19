#include "Settings.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cerrno>
#include <charconv>
#include <cstdlib>
#include <functional>
#include <sstream>

#ifndef CUBY_VERSION
#define CUBY_VERSION "0.0.0-dev"
#endif

namespace cuby {

namespace {

std::string lowercase(std::string_view text) {
    std::string out(text);
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

template <typename Enum, std::size_t N>
std::optional<Enum> parseEnum(const std::array<std::pair<std::string_view, Enum>, N>& table,
                              std::string_view text) {
    const std::string needle = lowercase(text);
    for (const auto& [name, value] : table) {
        if (name == needle) {
            return value;
        }
    }
    return std::nullopt;
}

constexpr std::array<std::pair<std::string_view, ColorMode>, 5> kColorModes{{
    {"rainbow", ColorMode::Rainbow},
    {"groups", ColorMode::Groups},
    {"random", ColorMode::Random},
    {"mono", ColorMode::Mono},
    {"fixed", ColorMode::Fixed},
}};

constexpr std::array<std::pair<std::string_view, Origin>, 6> kOrigins{{
    {"top", Origin::Top},
    {"bottom", Origin::Bottom},
    {"left", Origin::Left},
    {"right", Origin::Right},
    {"center", Origin::Center},
    {"random", Origin::Random},
}};

constexpr std::array<std::pair<std::string_view, Apparition>, 4> kApparitions{{
    {"fade", Apparition::Fade},
    {"rebound", Apparition::Rebound},
    {"slide", Apparition::Slide},
    {"instant", Apparition::Instant},
}};

constexpr std::array<std::pair<std::string_view, Shape>, 2> kShapes{{
    {"cube", Shape::Cube},
    {"flat", Shape::Flat},
}};

std::optional<long long> parseInteger(std::string_view text) {
    long long value = 0;
    const char* begin = text.data();
    const char* end = text.data() + text.size();
    const auto [ptr, ec] = std::from_chars(begin, end, value);
    if (ec != std::errc() || ptr != end || text.empty()) {
        return std::nullopt;
    }
    return value;
}

std::optional<double> parseReal(std::string_view text) {
    if (text.empty()) {
        return std::nullopt;
    }
    const std::string buffer(text);
    char* end = nullptr;
    errno = 0;
    const double value = std::strtod(buffer.c_str(), &end);
    if (errno != 0 || end == buffer.c_str() || *end != '\0') {
        return std::nullopt;
    }
    return value;
}

struct Option {
    std::string_view longName;
    std::string_view shortName;  // empty when there is none
    bool takesValue;
    std::string_view valueName;
    std::string_view help;
    // Returns an error message, or an empty string on success.
    std::function<std::string(Settings&, ParseOutcome&, std::string_view)> apply;
};

template <typename T>
std::string rangeError(std::string_view option, T lo, T hi) {
    std::ostringstream out;
    out << option << " must be between " << lo << " and " << hi;
    return out.str();
}

const std::vector<Option>& options() {
    static const std::vector<Option> table = {
        {"--width", "", true, "<px>", "window width in pixels (default 1024)",
         [](Settings& s, ParseOutcome&, std::string_view v) {
             const auto n = parseInteger(v);
             if (!n || *n < kMinWindowSize || *n > kMaxWindowSize) {
                 return rangeError("--width", kMinWindowSize, kMaxWindowSize);
             }
             s.width = static_cast<int>(*n);
             return std::string();
         }},
        {"--height", "", true, "<px>", "window height in pixels (default 600)",
         [](Settings& s, ParseOutcome&, std::string_view v) {
             const auto n = parseInteger(v);
             if (!n || *n < kMinWindowSize || *n > kMaxWindowSize) {
                 return rangeError("--height", kMinWindowSize, kMaxWindowSize);
             }
             s.height = static_cast<int>(*n);
             return std::string();
         }},
        {"--fullscreen", "-f", false, "", "start in (borderless) fullscreen",
         [](Settings& s, ParseOutcome&, std::string_view) {
             s.fullscreen = true;
             return std::string();
         }},
        {"--block-size", "-b", true, "<px>", "size of one block in pixels (default 40)",
         [](Settings& s, ParseOutcome&, std::string_view v) {
             const auto n = parseInteger(v);
             if (!n || *n < kMinBlockSize || *n > kMaxBlockSize) {
                 return rangeError("--block-size", kMinBlockSize, kMaxBlockSize);
             }
             s.blockSize = static_cast<int>(*n);
             return std::string();
         }},
        {"--gap", "", true, "<px>", "spacing between blocks in pixels (default 0)",
         [](Settings& s, ParseOutcome&, std::string_view v) {
             const auto n = parseInteger(v);
             if (!n || *n < 0 || *n > kMaxBlockSize) {
                 return rangeError("--gap", 0, kMaxBlockSize);
             }
             s.gap = static_cast<int>(*n);
             return std::string();
         }},
        {"--speed", "-s", true, "<factor>", "animation speed multiplier (default 1.0)",
         [](Settings& s, ParseOutcome&, std::string_view v) {
             const auto n = parseReal(v);
             if (!n || *n < kMinSpeed || *n > kMaxSpeed) {
                 return rangeError("--speed", kMinSpeed, kMaxSpeed);
             }
             s.speed = static_cast<float>(*n);
             return std::string();
         }},
        {"--color", "-c", true, "<mode>",
         "rainbow, groups, random, mono or a color name (default groups)",
         [](Settings& s, ParseOutcome&, std::string_view v) {
             if (const auto mode = parseColorMode(v); mode && *mode != ColorMode::Fixed) {
                 s.colorMode = *mode;
                 return std::string();
             }
             if (const auto group = findColorGroup(v)) {
                 s.colorMode = ColorMode::Fixed;
                 s.colorGroup = *group;
                 return std::string();
             }
             std::string names;
             for (const auto& group : colorGroups()) {
                 names += ' ';
                 names += group.name;
             }
             return "unknown color mode '" + std::string(v) +
                    "'. Modes: rainbow groups random mono. Colors:" + names;
         }},
        {"--origin", "-o", true, "<edge>", "top, bottom, left, right, center or random (default random)",
         [](Settings& s, ParseOutcome&, std::string_view v) {
             const auto origin = parseOrigin(v);
             if (!origin) {
                 return "unknown origin '" + std::string(v) +
                        "'. Use top, bottom, left, right, center or random";
             }
             s.origin = *origin;
             return std::string();
         }},
        {"--mode", "-m", true, "<mode>", "apparition: fade, rebound, slide or instant (default fade)",
         [](Settings& s, ParseOutcome&, std::string_view v) {
             const auto mode = parseApparition(v);
             if (!mode) {
                 return "unknown apparition mode '" + std::string(v) +
                        "'. Use fade, rebound, slide or instant";
             }
             s.apparition = *mode;
             return std::string();
         }},
        {"--shape", "", true, "<shape>", "cube or flat (default cube)",
         [](Settings& s, ParseOutcome&, std::string_view v) {
             const auto shape = parseShape(v);
             if (!shape) {
                 return "unknown shape '" + std::string(v) + "'. Use cube or flat";
             }
             s.shape = *shape;
             return std::string();
         }},
        {"--fps-limit", "", true, "<n>", "cap the frame rate, 0 = unlimited (default 60)",
         [](Settings& s, ParseOutcome&, std::string_view v) {
             const auto n = parseInteger(v);
             if (!n || *n < 0 || *n > 1000) {
                 return rangeError("--fps-limit", 0, 1000);
             }
             s.fpsLimit = static_cast<int>(*n);
             return std::string();
         }},
        {"--no-vsync", "", false, "", "do not synchronise rendering with the display",
         [](Settings& s, ParseOutcome&, std::string_view) {
             s.vsync = false;
             return std::string();
         }},
        {"--show-fps", "", false, "", "show the frame counter at start-up (F1 toggles it)",
         [](Settings& s, ParseOutcome&, std::string_view) {
             s.showFps = true;
             return std::string();
         }},
        {"--hide-cursor", "", false, "", "hide the mouse cursor over the window",
         [](Settings& s, ParseOutcome&, std::string_view) {
             s.hideCursor = true;
             return std::string();
         }},
        {"--exit-on-input", "", false, "", "quit on any key or mouse activity (screensaver behaviour)",
         [](Settings& s, ParseOutcome&, std::string_view) {
             s.exitOnInput = true;
             return std::string();
         }},
        {"--duration", "-d", true, "<seconds>", "quit after this many seconds (default 0 = never)",
         [](Settings& s, ParseOutcome&, std::string_view v) {
             const auto n = parseReal(v);
             if (!n || *n < 0.0) {
                 return std::string("--duration must be a positive number of seconds");
             }
             s.duration = *n;
             return std::string();
         }},
        {"--seed", "", true, "<n>", "seed the random generator for a reproducible run",
         [](Settings& s, ParseOutcome&, std::string_view v) {
             const auto n = parseInteger(v);
             if (!n || *n < 0 || *n > 0xFFFFFFFFLL) {
                 return std::string("--seed must be an integer between 0 and 4294967295");
             }
             s.seed = static_cast<std::uint32_t>(*n);
             return std::string();
         }},
        {"--font", "", true, "<path>", "TrueType font used for the on-screen text",
         [](Settings& s, ParseOutcome&, std::string_view v) {
             if (v.empty()) {
                 return std::string("--font needs a path");
             }
             s.fontPath = std::string(v);
             return std::string();
         }},
        {"--screenshot", "", true, "<file.bmp>", "save the last frame as a BMP image when quitting",
         [](Settings& s, ParseOutcome&, std::string_view v) {
             if (v.empty()) {
                 return std::string("--screenshot needs a path");
             }
             s.screenshotPath = std::string(v);
             return std::string();
         }},
        {"--help", "-h", false, "", "show this help and exit",
         [](Settings&, ParseOutcome& outcome, std::string_view) {
             outcome.action = ParseOutcome::Action::Exit;
             outcome.exitCode = 0;
             outcome.message = usageText();
             return std::string();
         }},
        {"--version", "-v", false, "", "show the version and exit",
         [](Settings&, ParseOutcome& outcome, std::string_view) {
             outcome.action = ParseOutcome::Action::Exit;
             outcome.exitCode = 0;
             outcome.message = versionText();
             return std::string();
         }},
    };
    return table;
}

const Option* findOption(std::string_view name) {
    for (const auto& option : options()) {
        if (option.longName == name || (!option.shortName.empty() && option.shortName == name)) {
            return &option;
        }
    }
    return nullptr;
}

}  // namespace

std::string_view toString(ColorMode mode) {
    for (const auto& [name, value] : kColorModes) {
        if (value == mode) {
            return name;
        }
    }
    return "?";
}

std::string_view toString(Origin origin) {
    for (const auto& [name, value] : kOrigins) {
        if (value == origin) {
            return name;
        }
    }
    return "?";
}

std::string_view toString(Apparition apparition) {
    for (const auto& [name, value] : kApparitions) {
        if (value == apparition) {
            return name;
        }
    }
    return "?";
}

std::string_view toString(Shape shape) {
    for (const auto& [name, value] : kShapes) {
        if (value == shape) {
            return name;
        }
    }
    return "?";
}

std::optional<ColorMode> parseColorMode(std::string_view text) { return parseEnum(kColorModes, text); }
std::optional<Origin> parseOrigin(std::string_view text) { return parseEnum(kOrigins, text); }
std::optional<Apparition> parseApparition(std::string_view text) { return parseEnum(kApparitions, text); }
std::optional<Shape> parseShape(std::string_view text) { return parseEnum(kShapes, text); }

ColorMode nextColorMode(ColorMode mode) {
    switch (mode) {
        case ColorMode::Rainbow:
            return ColorMode::Groups;
        case ColorMode::Groups:
            return ColorMode::Random;
        case ColorMode::Random:
            return ColorMode::Mono;
        case ColorMode::Mono:
        case ColorMode::Fixed:
            return ColorMode::Rainbow;
    }
    return ColorMode::Rainbow;
}

Origin nextOrigin(Origin origin) {
    switch (origin) {
        case Origin::Top:
            return Origin::Right;
        case Origin::Right:
            return Origin::Bottom;
        case Origin::Bottom:
            return Origin::Left;
        case Origin::Left:
            return Origin::Center;
        case Origin::Center:
            return Origin::Random;
        case Origin::Random:
            return Origin::Top;
    }
    return Origin::Top;
}

Apparition nextApparition(Apparition apparition) {
    switch (apparition) {
        case Apparition::Fade:
            return Apparition::Rebound;
        case Apparition::Rebound:
            return Apparition::Slide;
        case Apparition::Slide:
            return Apparition::Instant;
        case Apparition::Instant:
            return Apparition::Fade;
    }
    return Apparition::Fade;
}

Shape nextShape(Shape shape) { return shape == Shape::Cube ? Shape::Flat : Shape::Cube; }

const std::vector<ColorGroup>& colorGroups() {
    static const std::vector<ColorGroup> groups = {
        {"red", 0.0f, 0.95f},    {"orange", 28.0f, 0.95f}, {"yellow", 52.0f, 0.9f},
        {"green", 125.0f, 0.8f}, {"cyan", 185.0f, 0.85f},  {"blue", 225.0f, 0.85f},
        {"purple", 275.0f, 0.8f}, {"pink", 325.0f, 0.75f}, {"white", 0.0f, 0.0f},
    };
    return groups;
}

std::optional<int> findColorGroup(std::string_view name) {
    const std::string needle = lowercase(name);
    const auto& groups = colorGroups();
    for (std::size_t i = 0; i < groups.size(); ++i) {
        if (groups[i].name == needle) {
            return static_cast<int>(i);
        }
    }
    return std::nullopt;
}

ParseOutcome parseArguments(const std::vector<std::string_view>& args) {
    ParseOutcome outcome;

    auto fail = [&outcome](const std::string& message) {
        outcome.action = ParseOutcome::Action::Exit;
        outcome.exitCode = 2;
        outcome.message = "error: " + message + "\nTry 'cuby --help' for the list of options.";
        return outcome;
    };

    for (std::size_t i = 0; i < args.size(); ++i) {
        std::string_view arg = args[i];
        std::string_view inlineValue;
        bool hasInlineValue = false;

        if (arg.size() > 2 && arg.substr(0, 2) == "--") {
            if (const auto eq = arg.find('='); eq != std::string_view::npos) {
                inlineValue = arg.substr(eq + 1);
                hasInlineValue = true;
                arg = arg.substr(0, eq);
            }
        }

        const Option* option = findOption(arg);
        if (option == nullptr) {
            return fail("unknown option '" + std::string(arg) + "'");
        }

        std::string_view value;
        if (option->takesValue) {
            if (hasInlineValue) {
                value = inlineValue;
            } else if (i + 1 < args.size()) {
                value = args[++i];
            } else {
                return fail(std::string(option->longName) + " needs a value " + std::string(option->valueName));
            }
        } else if (hasInlineValue) {
            return fail(std::string(option->longName) + " does not take a value");
        }

        if (const std::string error = option->apply(outcome.settings, outcome, value); !error.empty()) {
            return fail(error);
        }
        if (outcome.action == ParseOutcome::Action::Exit) {
            return outcome;
        }
    }

    if (outcome.settings.gap >= outcome.settings.blockSize) {
        return fail("--gap must be smaller than --block-size");
    }
    return outcome;
}

std::string usageText() {
    std::ostringstream out;
    out << "Cuby " << CUBY_VERSION << " - a screensaver made of animated isometric cubes.\n\n"
        << "Usage: cuby [options]\n\nOptions:\n";

    for (const auto& option : options()) {
        std::string flags;
        if (!option.shortName.empty()) {
            flags += option.shortName;
            flags += ", ";
        } else {
            flags += "    ";
        }
        flags += option.longName;
        if (option.takesValue) {
            flags += ' ';
            flags += option.valueName;
        }
        out << "  " << flags;
        for (std::size_t pad = flags.size(); pad < 26; ++pad) {
            out << ' ';
        }
        out << ' ' << option.help << '\n';
    }

    out << "\nColor names:";
    for (const auto& group : colorGroups()) {
        out << ' ' << group.name;
    }
    out << "\n\nKeys:\n"
           "  Esc / Q         quit\n"
           "  Space / P       pause\n"
           "  F1              toggle the frame counter\n"
           "  F11 / F         toggle fullscreen\n"
           "  C               next color mode\n"
           "  M               next apparition mode\n"
           "  O               next origin\n"
           "  S               toggle cube / flat blocks\n"
           "  R               restart the current wave\n"
           "  + / - / wheel   change the animation speed\n";
    return out.str();
}

std::string versionText() { return std::string("cuby ") + CUBY_VERSION + "\n"; }

}  // namespace cuby
