# Build the server and the standalone simulation.
# make          build everything (builds the uSockets library first if needed)
# make test     run unit tests and integration tests
# make format   auto-format the source files
# make lint     check formatting and run static analysis
# make clean    remove build outputs

.RECIPEPREFIX := >

CXX      ?= g++
CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra
UWS_DIR  := third_party/uWebSockets
USOCKETS := $(UWS_DIR)/uSockets/uSockets.a
INCLUDES := -isystem $(UWS_DIR)/src -isystem $(UWS_DIR)/uSockets/src
SOURCES  := main.cpp sim_main.cpp world.h config.h log.h input.h origin.h tests/unit_tests.cpp

.PHONY: all test test-unit test-integration format lint clean

all: server sim

#The uSockets static library comes from the git submodule.
$(USOCKETS):
> $(MAKE) -C $(UWS_DIR)/uSockets

server: main.cpp world.h config.h log.h input.h origin.h $(USOCKETS)
> $(CXX) $(CXXFLAGS) $(INCLUDES) main.cpp $(USOCKETS) -lz -o server

sim: sim_main.cpp world.h
> $(CXX) $(CXXFLAGS) sim_main.cpp -o sim

# doctest is a single header kept in tests.
tests/unit_tests: tests/unit_tests.cpp tests/doctest.h world.h config.h input.h origin.h
> $(CXX) $(CXXFLAGS) -I. -isystem tests tests/unit_tests.cpp -o tests/unit_tests

test-unit: tests/unit_tests
> ./tests/unit_tests

#Needs Node 18+. Starts the real server binary and talks to it over WebSocket.
test-integration: server
> cd tests/integration && npm ci --silent && node --test integration.test.js

test: test-unit test-integration

format:
> clang-format -i $(SOURCES)

lint:
> clang-format --dry-run --Werror $(SOURCES)
> clang-tidy -quiet main.cpp sim_main.cpp -checks='-*,bugprone-*,performance-*,-bugprone-easily-swappable-parameters' -header-filter='(world|config|log|input|origin)\.h$$' -- $(CXXFLAGS) $(INCLUDES)

clean:
> rm -f server sim tests/unit_tests