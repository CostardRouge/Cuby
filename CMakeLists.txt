cmake_minimum_required(VERSION 3.16)

project(Cuby
    VERSION 0.2.0
    DESCRIPTION "A screensaver made of animated isometric cubes"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(CUBY_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(CUBY_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(CUBY_BUILD_TESTS "Build the tests" ON)

include(GNUInstallDirs)

# --- Dependencies -----------------------------------------------------------

find_package(SDL2 CONFIG REQUIRED)
if(TARGET SDL2::SDL2)
    set(CUBY_SDL2_TARGET SDL2::SDL2)
else()
    # Older SDL2 packages only export variables.
    add_library(cuby_sdl2 INTERFACE)
    target_include_directories(cuby_sdl2 INTERFACE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(cuby_sdl2 INTERFACE ${SDL2_LIBRARIES})
    set(CUBY_SDL2_TARGET cuby_sdl2)
endif()

find_package(SDL2_ttf CONFIG QUIET)
if(TARGET SDL2_ttf::SDL2_ttf)
    set(CUBY_SDL2_TTF_TARGET SDL2_ttf::SDL2_ttf)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2_TTF REQUIRED IMPORTED_TARGET SDL2_ttf)
    set(CUBY_SDL2_TTF_TARGET PkgConfig::SDL2_TTF)
endif()

# --- Shared compile settings -------------------------------------------------

add_library(cuby_options INTERFACE)
if(MSVC)
    target_compile_options(cuby_options INTERFACE /W4 /permissive-)
    if(CUBY_WARNINGS_AS_ERRORS)
        target_compile_options(cuby_options INTERFACE /WX)
    endif()
else()
    target_compile_options(cuby_options INTERFACE -Wall -Wextra -Wpedantic -Wshadow -Wconversion)
    if(CUBY_WARNINGS_AS_ERRORS)
        target_compile_options(cuby_options INTERFACE -Werror)
    endif()
    if(CUBY_SANITIZE)
        target_compile_options(cuby_options INTERFACE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(cuby_options INTERFACE -fsanitize=address,undefined)
    endif()
endif()

target_compile_definitions(cuby_options INTERFACE
    CUBY_VERSION="${PROJECT_VERSION}"
    CUBY_SOURCE_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/Assets"
    CUBY_INSTALL_ASSET_DIR="${CMAKE_INSTALL_FULL_DATADIR}/cuby")

# --- Library with the application logic (shared by the executable and tests) --

add_library(cuby_core STATIC
    src/App.cpp
    src/Display.cpp
    src/Grid.cpp
    src/Overlay.cpp
    src/Palette.cpp
    src/Settings.cpp)
target_include_directories(cuby_core PUBLIC src)
target_link_libraries(cuby_core
    PUBLIC ${CUBY_SDL2_TARGET} ${CUBY_SDL2_TTF_TARGET}
    PRIVATE cuby_options)

# --- Executable ---------------------------------------------------------------

add_executable(cuby src/main.cpp)
target_link_libraries(cuby PRIVATE cuby_core cuby_options)
if(TARGET SDL2::SDL2main)
    target_link_libraries(cuby PRIVATE SDL2::SDL2main)
endif()

install(TARGETS cuby RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES Assets/font.ttf DESTINATION ${CMAKE_INSTALL_DATADIR}/cuby)

# --- Tests --------------------------------------------------------------------

if(CUBY_BUILD_TESTS)
    enable_testing()

    add_executable(cuby_tests tests/unit_tests.cpp)
    target_link_libraries(cuby_tests PRIVATE cuby_core cuby_options)
    add_test(NAME unit COMMAND cuby_tests)

    # Headless smoke runs: SDL's dummy video driver needs no display.
    set(CUBY_HEADLESS_ENV "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

    add_test(NAME smoke_help COMMAND cuby --help)
    add_test(NAME smoke_bad_option COMMAND cuby --block-size 1)
    set_tests_properties(smoke_bad_option PROPERTIES WILL_FAIL TRUE)

    foreach(shape cube flat)
        foreach(mode fade rebound slide instant)
            add_test(NAME smoke_${shape}_${mode}
                COMMAND cuby --duration 0.4 --fps-limit 0 --no-vsync --show-fps --seed 7
                        --shape ${shape} --mode ${mode} --speed 8 --width 320 --height 200)
            set_tests_properties(smoke_${shape}_${mode} PROPERTIES ENVIRONMENT "${CUBY_HEADLESS_ENV}")
        endforeach()
    endforeach()

    add_test(NAME smoke_screenshot
        COMMAND cuby --duration 0.3 --fps-limit 0 --no-vsync --seed 9 --width 320 --height 200
                --screenshot "${CMAKE_CURRENT_BINARY_DIR}/smoke_screenshot.bmp")
    set_tests_properties(smoke_screenshot PROPERTIES
        ENVIRONMENT "${CUBY_HEADLESS_ENV}"
        PASS_REGULAR_EXPRESSION "screenshot saved to")

    foreach(color rainbow groups random mono blue)
        add_test(NAME smoke_color_${color}
            COMMAND cuby --duration 0.3 --fps-limit 0 --no-vsync --seed 3 --color ${color}
                    --origin center --width 320 --height 200)
        set_tests_properties(smoke_color_${color} PROPERTIES ENVIRONMENT "${CUBY_HEADLESS_ENV}")
    endforeach()
endif()
