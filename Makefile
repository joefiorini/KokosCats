# If RACK_DIR is not defined when calling the Makefile, default to two directories above
RACK_DIR ?= ../..

# FLAGS will be passed to both the C and C++ compiler
FLAGS += -Isrc
CFLAGS +=
CXXFLAGS +=

# Careful about linking to shared libraries, since you can't assume much about the user's environment and library search path.
# Static libraries are fine, but they should be added to this plugin's build system.
LDFLAGS +=

# Add .cpp files to the build
SOURCES += $(wildcard src/*.cpp)

# Add files to the ZIP package when running `make dist`
# The compiled plugin and "plugin.json" are automatically added.
DISTRIBUTABLES += res
DISTRIBUTABLES += $(wildcard LICENSE*)
DISTRIBUTABLES += $(wildcard presets)

# Include the Rack plugin Makefile framework
include $(RACK_DIR)/plugin.mk

########################################################################
#
# Build and run the tests
#
########################################################################

DHEUNIT_SRC = dheunit
DHEUNIT_INCLUDE_DIR = $(DHEUNIT_SRC)/dheunit

$(DHEUNIT_INCLUDE_DIR):
	git submodule update --init --recursive

RACK_INCLUDES = -I$(RACK_DIR)/include -I$(RACK_DIR)/dep/include
TEST_INCLUDES =  -Itest -I$(DHEUNIT_SRC)
TEST_CXXFLAGS = $(filter-out $(RACK_INCLUDES),$(CXXFLAGS)) $(TEST_INCLUDES) 

TEST_SOURCES = $(shell find test -name "*.cpp")

TEST_OBJECTS := $(patsubst %, build/%.o, $(TEST_SOURCES))
-include $(TEST_OBJECTS:.o=.d)

$(TEST_OBJECTS): $(DHEUNIT_INCLUDE_DIR)
$(TEST_OBJECTS): CXXFLAGS := $(TEST_CXXFLAGS)

TEST_RUNNER = build/dheunit

$(TEST_RUNNER): $(TEST_OBJECTS)
	@mkdir -p $(@D)
	$(CXX) -o $@ $^

.PHONY: test vtest

test: $(TEST_RUNNER)
	$<

vtest: $(TEST_RUNNER)
	$< --verbose
