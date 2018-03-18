# by default, bmake will cd into ./obj first
.OBJDIR: ./

# Default install prefix
PREFIX?=/usr/local

# Use ninja, if it is installed
_GENERATOR!=which ninja 2>/dev/null >/dev/null && echo Ninja || echo "'Unix Makefiles'"
GENERATOR?=$(_GENERATOR)

# Figure out if we can build with CMake, GNU Make, autoconf then GNU Make, or not at all
BUILDFILE!=(which cmake >/dev/null 2>/dev/null && echo "build/fish") || ((test -e ./configure || (which autoreconf >/dev/null 2>/dev/null && test -e ./configure.ac)) && echo "./fish") || echo ".nobuild"

.DEFAULT: $(BUILDFILE)
DEFAULT: $(BUILDFILE)

### CMAKE TARGETS ###
build/fish: build/.cmake
	cmake --build build

build/.cmake: build
	cd build; cmake .. -G $(GENERATOR) -DCMAKE_INSTALL_PREFIX=$(PREFIX) -DCMAKE_EXPORT_COMPILE_COMMANDS=1
	touch build/.cmake

build:
	mkdir -p build

.PHONY: install
install: $(BUILDFILE)
	# only execute this if we didn't shell out to autoconf/gmake
	test "$(BUILDFILE)" != "build/fish" || \
	cmake --build build --target install

.PHONY: clean
clean: $(BUILDFILE)
	# only execute this if we didn't shell out to autoconf/gmake
	test "$(BUILDFILE)" != "build/fish" || \
	rm -rf build

.PHONY: test
test: build/fish
	# only execute this if we didn't shell out to autoconf/gmake
	test "$(BUILDFILE)" != "build/fish" || \
	cmake --build build --target test

.PHONY: run
run: build/fish
	# only execute this if we didn't shell out to autoconf/gmake
	test "$(BUILDFILE)" != "build/fish" || \
	build/fish

### NO BUILD TOOLS INSTALLED ###
.PHONY: .nobuild
.nobuild:
	# cmake takes care of checking for all other dependencies
	echo "Please install cmake and then re-run `Make` to build" 1>&2

### GNU AUTOCONF TARGETS ###
./fish: ./GNUmakefile
	+$(MAKE) $(MAKECMDGOALS)

./configure: ./configure.ac
	autoreconf --no-recursive

./GNUmakefile: ./configure
	./configure --prefix=$(PREFIX)

