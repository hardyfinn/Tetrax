# Tetrax for VCV Rack.
# Build:  export RACK_DIR=/path/to/Rack-SDK && make && make install   (or make dist)
# Tests:  make test   (host build of the DSP engines, no Rack SDK needed)

RACK_DIR ?= ../..

CXXFLAGS += -Wall -Wextra $(EXTRA_CXXFLAGS)

SOURCES += $(wildcard src/*.cpp src/modules/*.cpp)

DISTRIBUTABLES += res
DISTRIBUTABLES += $(wildcard LICENSE*)
DISTRIBUTABLES += README.md

ifneq ($(wildcard $(RACK_DIR)/plugin.mk),)
include $(RACK_DIR)/plugin.mk
endif

TEST_CXX ?= g++
build/tetrax_tests: $(wildcard tests/*.cpp tests/*.hpp src/dsp/*.hpp)
	@mkdir -p build
	$(TEST_CXX) -std=c++17 -O3 -funsafe-math-optimizations -Wall -Wextra -o $@ $(wildcard tests/*.cpp)

.PHONY: test panels tables render
test: build/tetrax_tests
	./build/tetrax_tests

# Regenerate res/*.svg and src/generated/Layout.hpp (needs Python 3 and fonttools).
panels:
	python3 scripts/make_panels.py

# Regenerate src/dsp/ModeTables.hpp (needs numpy and scipy).
tables:
	python3 scripts/make_mode_tables.py

# Render every factory preset to build/render/*.wav
render: build/tetrax_tests
	@mkdir -p build/render
	./build/tetrax_tests --render
