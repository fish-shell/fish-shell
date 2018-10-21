# by default bmake will cd into ./obj first
.OBJDIR: ./

.BEGIN:
	# test for cmake, which is the only requirement to be able to run this Makefile
	# cmake will perform the remaining dependency tests on its own
	@which cmake >/dev/null 2>/dev/null || (echo 'Please install cmake and then re-run the `make` command!' 1>&2 && false)

# Use ninja, if it is installed
_GENERATOR!=which ninja 2>/dev/null >/dev/null && echo Ninja || echo "'Unix Makefiles'"
GENERATOR?=$(_GENERATOR)
PREFIX?=/usr/local

.if $(GENERATOR) == "Ninja"
BUILDFILE=build/build.ninja
.else
BUILDFILE=build/Makefile
.endif

.DEFAULT: build/fish
build/fish: build/$(BUILDFILE)
	cmake --build build

build:
	mkdir -p build

build/$(BUILDFILE): build
	cd build; cmake .. -G $(GENERATOR) -DCMAKE_INSTALL_PREFIX=$(PREFIX) -DCMAKE_EXPORT_COMPILE_COMMANDS=1

.PHONY: install
install: build/fish
	cmake --build build --target install

.PHONY: clean
clean:
	rm -rf build

.PHONY: test
test: build/fish
	cmake --build build --target test

.PHONY: run
run: build/fish
	build/fish
