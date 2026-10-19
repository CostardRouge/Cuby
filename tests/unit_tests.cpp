// Minimal self-contained tests for the parts of Cuby that do not need a window.
// Run with `ctest` or directly; exits non-zero on the first failure.

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include "Palette.hpp"
#include "Random.hpp"
#include "Settings.hpp"

namespace {

int failures = 0;

void check(bool condition, const char* expression, const char* file, int line) {
    if (!condition) {
        ++failures;
        std::cerr << file << ":" << line << ": check failed: " << expression << std::endl;
    }
}

#define CHECK(expr) check((expr), #expr, __FILE__, __LINE__)

cuby::ParseOutcome parse(std::initializer_list<std::string_view> args) {
    return cuby::parseArguments(std::vector<std::string_view>(args));
}

void testDefaults() {
    const auto outcome = parse({});
    CHECK(outcome.action == cuby::ParseOutcome::Action::Run);
    CHECK(outcome.settings.width == 1024);
    CHECK(outcome.settings.height == 600);
    CHECK(outcome.settings.blockSize == 40);
    CHECK(outcome.settings.shape == cuby::Shape::Cube);
    CHECK(outcome.settings.colorMode == cuby::ColorMode::Groups);
    CHECK(!outcome.settings.fullscreen);
    CHECK(outcome.settings.vsync);
}

void testValues() {
    const auto outcome = parse({"--width", "800", "--height=480", "-b", "16", "--gap", "2", "-s", "2.5",
                                "--origin", "LEFT", "-m", "slide", "--shape", "flat", "--fps-limit", "0",
                                "--no-vsync", "--show-fps", "--hide-cursor", "--exit-on-input",
                                "--duration", "12.5", "--seed", "42", "--font", "x.ttf", "-f"});
    CHECK(outcome.action == cuby::ParseOutcome::Action::Run);
    const cuby::Settings& s = outcome.settings;
    CHECK(s.width == 800);
    CHECK(s.height == 480);
    CHECK(s.blockSize == 16);
    CHECK(s.gap == 2);
    CHECK(s.speed == 2.5f);
    CHECK(s.origin == cuby::Origin::Left);
    CHECK(s.apparition == cuby::Apparition::Slide);
    CHECK(s.shape == cuby::Shape::Flat);
    CHECK(s.fpsLimit == 0);
    CHECK(!s.vsync);
    CHECK(s.showFps);
    CHECK(s.hideCursor);
    CHECK(s.exitOnInput);
    CHECK(s.duration == 12.5);
    CHECK(s.seed.has_value() && *s.seed == 42u);
    CHECK(s.fontPath == "x.ttf");
    CHECK(s.fullscreen);
}

void testColorOption() {
    auto outcome = parse({"--color", "rainbow"});
    CHECK(outcome.settings.colorMode == cuby::ColorMode::Rainbow);

    outcome = parse({"-c", "Blue"});
    CHECK(outcome.action == cuby::ParseOutcome::Action::Run);
    CHECK(outcome.settings.colorMode == cuby::ColorMode::Fixed);
    CHECK(cuby::findColorGroup("blue").has_value());
    CHECK(outcome.settings.colorGroup == *cuby::findColorGroup("blue"));

    outcome = parse({"--color", "fixed"});  // only reachable through a color name
    CHECK(outcome.action == cuby::ParseOutcome::Action::Exit);
    CHECK(outcome.exitCode == 2);

    outcome = parse({"--color", "plaid"});
    CHECK(outcome.action == cuby::ParseOutcome::Action::Exit);
    CHECK(outcome.exitCode == 2);
}

void testErrors() {
    auto outcome = parse({"--bogus"});
    CHECK(outcome.action == cuby::ParseOutcome::Action::Exit);
    CHECK(outcome.exitCode == 2);
    CHECK(outcome.message.find("unknown option") != std::string::npos);

    outcome = parse({"--width"});
    CHECK(outcome.exitCode == 2);

    outcome = parse({"--width", "abc"});
    CHECK(outcome.exitCode == 2);

    outcome = parse({"--block-size", "2"});
    CHECK(outcome.exitCode == 2);

    outcome = parse({"--speed", "0"});
    CHECK(outcome.exitCode == 2);

    outcome = parse({"--gap", "40"});  // equal to the default block size
    CHECK(outcome.exitCode == 2);

    outcome = parse({"--fullscreen=yes"});
    CHECK(outcome.exitCode == 2);

    outcome = parse({"--seed", "-1"});
    CHECK(outcome.exitCode == 2);
}

void testHelpAndVersion() {
    auto outcome = parse({"--help"});
    CHECK(outcome.action == cuby::ParseOutcome::Action::Exit);
    CHECK(outcome.exitCode == 0);
    CHECK(outcome.message.find("--block-size") != std::string::npos);
    CHECK(outcome.message.find("Keys:") != std::string::npos);

    outcome = parse({"-v"});
    CHECK(outcome.exitCode == 0);
    CHECK(outcome.message.rfind("cuby ", 0) == 0);

    // Options after --help are ignored, even invalid ones.
    outcome = parse({"-h", "--width", "nope"});
    CHECK(outcome.exitCode == 0);
}

void testEnumRoundTrip() {
    for (auto mode : {cuby::ColorMode::Rainbow, cuby::ColorMode::Groups, cuby::ColorMode::Random,
                      cuby::ColorMode::Mono, cuby::ColorMode::Fixed}) {
        CHECK(cuby::parseColorMode(cuby::toString(mode)) == mode);
    }
    for (auto origin : {cuby::Origin::Top, cuby::Origin::Bottom, cuby::Origin::Left, cuby::Origin::Right,
                        cuby::Origin::Center, cuby::Origin::Random}) {
        CHECK(cuby::parseOrigin(cuby::toString(origin)) == origin);
    }
    for (auto apparition : {cuby::Apparition::Fade, cuby::Apparition::Rebound, cuby::Apparition::Slide,
                            cuby::Apparition::Instant}) {
        CHECK(cuby::parseApparition(cuby::toString(apparition)) == apparition);
    }
    CHECK(cuby::parseShape("CUBE") == cuby::Shape::Cube);
    CHECK(!cuby::parseShape("sphere").has_value());

    // Cycling visits every value and comes back to the start.
    cuby::Origin origin = cuby::Origin::Top;
    int steps = 0;
    do {
        origin = cuby::nextOrigin(origin);
        ++steps;
    } while (origin != cuby::Origin::Top && steps < 100);
    CHECK(steps == 6);

    cuby::Apparition apparition = cuby::Apparition::Fade;
    steps = 0;
    do {
        apparition = cuby::nextApparition(apparition);
        ++steps;
    } while (apparition != cuby::Apparition::Fade && steps < 100);
    CHECK(steps == 4);

    CHECK(cuby::nextColorMode(cuby::ColorMode::Fixed) == cuby::ColorMode::Rainbow);
    CHECK(cuby::nextShape(cuby::nextShape(cuby::Shape::Cube)) == cuby::Shape::Cube);
}

void testColorMaths() {
    const SDL_Color red = cuby::hsvToRgb(0.0f, 1.0f, 1.0f);
    CHECK(red.r == 255 && red.g == 0 && red.b == 0 && red.a == 255);

    const SDL_Color green = cuby::hsvToRgb(120.0f, 1.0f, 1.0f);
    CHECK(green.r == 0 && green.g == 255 && green.b == 0);

    const SDL_Color blue = cuby::hsvToRgb(240.0f + 360.0f, 1.0f, 1.0f);  // hue wraps
    CHECK(blue.r == 0 && blue.g == 0 && blue.b == 255);

    const SDL_Color grey = cuby::hsvToRgb(37.0f, 0.0f, 0.5f);
    CHECK(grey.r == grey.g && grey.g == grey.b);

    const SDL_Color mixed = cuby::mix(SDL_Color{0, 0, 0, 0}, SDL_Color{200, 100, 50, 255}, 0.5f);
    CHECK(mixed.r == 100 && mixed.g == 50 && mixed.b == 25 && mixed.a == 128);

    const SDL_Color dark = cuby::shade(SDL_Color{200, 100, 50, 77}, 0.5f);
    CHECK(dark.r == 100 && dark.g == 50 && dark.b == 25 && dark.a == 77);
}

void testPalette() {
    cuby::Random rng(123u);
    cuby::Palette palette(rng);

    palette.beginWave(cuby::ColorMode::Fixed, *cuby::findColorGroup("red"));
    CHECK(palette.groupName() == "red");
    for (int i = 0; i < 50; ++i) {
        const SDL_Color c = palette.colorAt(rng.unit(), rng.unit());
        CHECK(c.a == 255);
        CHECK(c.r >= c.g && c.r >= c.b);  // red family stays red-dominant
    }

    palette.beginWave(cuby::ColorMode::Mono, 0);
    for (int i = 0; i < 20; ++i) {
        const SDL_Color c = palette.colorAt(0.5f, 0.5f);
        CHECK(c.r == c.g && c.g == c.b);
    }

    // The Groups mode never repeats the same family twice in a row.
    palette.beginWave(cuby::ColorMode::Groups, 0);
    for (int i = 0; i < 30; ++i) {
        const std::string before(palette.groupName());
        palette.beginWave(cuby::ColorMode::Groups, 0);
        CHECK(before != palette.groupName());
    }
}

void testRandom() {
    cuby::Random a(99u);
    cuby::Random b(99u);
    for (int i = 0; i < 100; ++i) {
        CHECK(a.between(-5, 5) == b.between(-5, 5));
        const int value = a.between(10, 3);  // reversed bounds are accepted
        CHECK(value >= 3 && value <= 10);
        b.between(10, 3);
        const float unit = a.unit();
        CHECK(unit >= 0.0f && unit < 1.0f);
        b.unit();
    }
    CHECK(a.seed() == 99u);
}

}  // namespace

int main() {
    testDefaults();
    testValues();
    testColorOption();
    testErrors();
    testHelpAndVersion();
    testEnumRoundTrip();
    testColorMaths();
    testPalette();
    testRandom();

    if (failures != 0) {
        std::cerr << failures << " check(s) failed" << std::endl;
        return EXIT_FAILURE;
    }
    std::cout << "all checks passed" << std::endl;
    return EXIT_SUCCESS;
}
