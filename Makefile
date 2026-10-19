##
## Convenience wrapper around the CMake build.
##
##   make            build (Release) into ./build
##   make run        build then start cuby (pass options with ARGS="...")
##   make test       build then run the unit and headless smoke tests
##   make debug      build with sanitizers and warnings as errors
##   make clean      remove the build directory
##

BUILD_DIR	?=	build
BUILD_TYPE	?=	Release
CMAKE_ARGS	?=
ARGS		?=

.PHONY: all build run test debug install clean re

all:		build

build:
		cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) $(CMAKE_ARGS)
		cmake --build $(BUILD_DIR) --parallel

run:		build
		./$(BUILD_DIR)/cuby $(ARGS)

test:		build
		ctest --test-dir $(BUILD_DIR) --output-on-failure

debug:
		$(MAKE) build BUILD_TYPE=Debug CMAKE_ARGS="-DCUBY_SANITIZE=ON -DCUBY_WARNINGS_AS_ERRORS=ON $(CMAKE_ARGS)"

install:	build
		cmake --install $(BUILD_DIR)

clean:
		rm -rf $(BUILD_DIR)

re:		clean all
