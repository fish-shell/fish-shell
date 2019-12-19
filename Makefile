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

all: .begin build/fish

PHONY: .begin
.begin:
	@which $(CMAKE) > /dev/null 2> /dev/null || \
		 (echo 'Please install CMake and then re-run the `make` command!' 1>&2 && false)

build/fish: build/$(BUILDFILE)
	$(CMAKE) --build build

build/$(BUILDFILE): build
	cd build; $(CMAKE) .. -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -G "$(GENERATOR)" -DCMAKE_INSTALL_PREFIX="$(PREFIX)" -DCMAKE_EXPORT_COMPILE_COMMANDS=1

build:
	mkdir -p build

.PHONY: clean
clean:
	rm -rf build

.PHONY: test
test: build/fish
	$(CMAKE) --build build --target test

.PHONY: install
install: build/fish
	$(CMAKE) --build build --target install

.PHONY: run
run: build/fish
	./build/fish || true

.PHONY: exec
exec: build/fish
	exec ./build/fish
