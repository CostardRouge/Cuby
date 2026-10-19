#include <SDL.h>

#include <exception>
#include <iostream>
#include <string_view>
#include <vector>

#include "App.hpp"
#include "Exception.hpp"
#include "Settings.hpp"

namespace {

constexpr const char* kTag = "[Cuby]";

}  // namespace

int main(int argc, char* argv[]) {
    std::vector<std::string_view> args;
    args.reserve(static_cast<std::size_t>(argc > 0 ? argc - 1 : 0));
    for (int i = 1; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }

    const cuby::ParseOutcome outcome = cuby::parseArguments(args);
    if (outcome.action == cuby::ParseOutcome::Action::Exit) {
        (outcome.exitCode == 0 ? std::cout : std::cerr) << outcome.message;
        if (!outcome.message.empty() && outcome.message.back() != '\n') {
            (outcome.exitCode == 0 ? std::cout : std::cerr) << '\n';
        }
        return outcome.exitCode;
    }

    try {
        cuby::App app(outcome.settings);
        return app.run();
    } catch (const cuby::Exception& e) {
        std::cerr << kTag << " error: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << kTag << " unexpected error: " << e.what() << std::endl;
    }
    return 1;
}
