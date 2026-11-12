# Build the server and the standalone simulation.
# make          build everything (builds the uSockets library first if needed)
# make format   auto-format the source files
# make lint     check formatting and run static analysis
# make clean    remove build outputs

.RECIPEPREFIX := >

CXX      ?= g++
CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra
UWS_DIR  := third_party/uWebSockets
USOCKETS := $(UWS_DIR)/uSockets/uSockets.a
INCLUDES := -isystem $(UWS_DIR)/src -isystem $(UWS_DIR)/uSockets/src
SOURCES  := main.cpp sim_main.cpp world.h config.h log.h

.PHONY: all format lint clean

all: server sim

# The uSockets static library comes from the git submodule.
$(USOCKETS):
> $(MAKE) -C $(UWS_DIR)/uSockets

server: main.cpp world.h config.h log.h $(USOCKETS)
> $(CXX) $(CXXFLAGS) $(INCLUDES) main.cpp $(USOCKETS) -lz -o server

sim: sim_main.cpp world.h
> $(CXX) $(CXXFLAGS) sim_main.cpp -o sim

format:
> clang-format -i $(SOURCES)

lint:
> clang-format --dry-run --Werror $(SOURCES)
> clang-tidy main.cpp sim_main.cpp -checks='-*,bugprone-*,performance-*,-bugprone-easily-swappable-parameters' -header-filter='(world|config|log)\.h$$' -- $(CXXFLAGS) $(INCLUDES)

clean:
> rm -f server sim