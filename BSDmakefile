# This is a very basic `make` wrapper around the CMake build toolchain.
#
# Supported arguments:
#   PREFIX: sets the installation prefix
#   GENERATOR: explicitly specifies the CMake generator to use

# By default, bmake will try to cd into ./obj before anything else. Don't do that.
.OBJDIR: ./

CMAKE?=cmake

# Before anything else, test for CMake, which is the only requirement to be able to run
# this Makefile CMake will perform the remaining dependency tests on its own.
.BEGIN:
	@which $(CMAKE) >/dev/null 2>/dev/null || \
		(echo 'Please install CMake and then re-run the `make` command!' 1>&2 && false)

# Prefer to use ninja, if it is installed
_GENERATOR!=which ninja 2>/dev/null >/dev/null && echo Ninja || echo "Unix Makefiles"
GENERATOR?=$(_GENERATOR)

.if $(GENERATOR) == "Ninja"
BUILDFILE=build.ninja
.else
BUILDFILE=Makefile
.endif

PREFIX?=/usr/local

.PHONY: build/ghoti
build/ghoti: build/$(BUILDFILE)
	$(CMAKE) --build build

# Don't split the mkdir into its own rule because that would cause CMake to regenerate the build 
# files after each build (because it adds the mdate of the build directory into the out-of-date
# calculation tree). GNUmake supports order-only dependencies, BSDmake does not seem to.
build/$(BUILDFILE):
	mkdir -p build
	cd build; $(CMAKE) .. -G "$(GENERATOR)" -DCMAKE_INSTALL_PREFIX="$(PREFIX)" -DCMAKE_EXPORT_COMPILE_COMMANDS=1

.PHONY: install
install: build/ghoti
	$(CMAKE) --build build --target install

.PHONY: clean
clean:
	rm -rf build

.PHONY: test
test: build/ghoti
	$(CMAKE) --build build --target test

.PHONY: run
run: build/ghoti
	build/ghoti
