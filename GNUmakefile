# This is a very basic `make` wrapper around the CMake build toolchain.
#
# Supported arguments:
#   PREFIX: sets the installation prefix
#   GENERATOR: explicitly specifies the CMake generator to use

CMAKE ?= cmake

GENERATOR ?= $(shell (which ninja > /dev/null 2> /dev/null && echo Ninja) || \
	echo 'Unix Makefiles')
prefix ?= /usr/local
PREFIX ?= $(prefix)

ifeq ($(GENERATOR), Ninja)
BUILDFILE = build.ninja
else
BUILDFILE = Makefile
endif


# If CMake has generated an in-tree Makefile, use that instead (issue #6264)
MAKE_DIR:=$(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))
ifeq ($(shell test -f $(MAKE_DIR)/Makefile && echo 1), 1)

all:
	@+$(MAKE) -f $(MAKE_DIR)/Makefile $(MAKECMDGOALS) --no-print-directory
%:
	@+$(MAKE) -f $(MAKE_DIR)/Makefile $(MAKECMDGOALS) --no-print-directory

else

all: .begin build/fish

.PHONY: .begin
.begin:
	@which $(CMAKE) > /dev/null 2> /dev/null || \
		(echo 'Please install CMake and then re-run the `make` command!' 1>&2 && false)

.PHONY: build/fish
build/fish: build/$(BUILDFILE)
	$(CMAKE) --build build

# Use build as an order-only dependency. This prevents the target from always being outdated
# after a make run, and more importantly, doesn't clobber manually specified CMake options.
build/$(BUILDFILE): | build
	cd build; $(CMAKE) .. -G "$(GENERATOR)" \
		-DCMAKE_INSTALL_PREFIX="$(PREFIX)" -DCMAKE_EXPORT_COMPILE_COMMANDS=1

build:
	mkdir -p build

.PHONY: clean
clean:
	rm -rf build

.PHONY: test
test: build/fish
	$(CMAKE) --build build --target fish_run_tests

.PHONY: fish_run_tests
fish_run_tests: build/fish
	$(CMAKE) --build build --target fish_run_tests

.PHONY: install
install: build/fish
	$(CMAKE) --build build --target install

.PHONY: run
run: build/fish
	./build/fish || true

.PHONY: exec
exec: build/fish
	exec ./build/fish

endif # CMake in-tree build check
