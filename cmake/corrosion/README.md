# Corrosion
[![Build Status](https://github.com/corrosion-rs/corrosion/workflows/.github/workflows/test.yaml/badge.svg)](https://github.com/corrosion-rs/corrosion/actions?query=branch%3Amaster)

Corrosion, formerly known as cmake-cargo, is a tool for integrating Rust into an existing CMake
project. Corrosion is capable of importing executables, static libraries, and dynamic libraries
from a crate.

## Features
- Automatic Import of Executable, Static, and Shared Libraries from Rust Crate
- Easy Installation of Rust Executables
- Trivially Link Rust Executables to C/C++ Libraries in Tree
- Multi-Config Generator Support
- Simple Cross-Compilation

## Sample Usage
```cmake
cmake_minimum_required(VERSION 3.15)
project(MyCoolProject LANGUAGES CXX)

find_package(Corrosion REQUIRED)

corrosion_import_crate(MANIFEST_PATH rust-lib/Cargo.toml)

add_executable(cpp-exe main.cpp)
target_link_libraries(cpp-exe PUBLIC rust-lib)
```

# Documentation

## Table of Contents
- [Corrosion](#corrosion)
  - [Features](#features)
  - [Sample Usage](#sample-usage)
- [Documentation](#documentation)
  - [Table of Contents](#table-of-contents)
  - [Installation](#installation)
    - [Package Manager](#package-manager)
    - [CMake Install](#cmake-install)
    - [FetchContent](#fetchcontent)
    - [Subdirectory](#subdirectory)
  - [Usage](#usage)
    - [Corrosion Options](#corrosion-options)
      - [Developer/Maintainer Options](#developermaintainer-options)
    - [Information provided by Corrosion](#information-provided-by-corrosion)
    - [Adding crate targets](#adding-crate-targets)
    - [Per Target options](#per-target-options)
    - [Selecting a custom cargo profile](#selecting-a-custom-cargo-profile)
    - [Importing C-Style Libraries Written in Rust](#importing-c-style-libraries-written-in-rust)
      - [Generate Bindings to Rust Library Automatically](#generate-bindings-to-rust-library-automatically)
    - [Importing Libraries Written in C and C++ Into Rust](#importing-libraries-written-in-c-and-c-into-rust)
    - [Cross Compiling](#cross-compiling)
      - [Windows-to-Windows](#windows-to-windows)
      - [Linux-to-Linux](#linux-to-linux)
      - [Android](#android)

## Installation
There are two fundamental installation methods that are supported by Corrosion - installation as a
CMake package or using it as a subdirectory in an existing CMake project. For CMake versions below
3.19 Corrosion strongly recommends installing the package, either via a package manager or manually
using CMake's installation facilities.

Installation will pre-build all of Corrosion's native tooling (required only for CMake versions
below 3.19).
Using Corrosion as a subdirectory with CMake versions before 3.19 will result in the native
tooling for Corrosion to be re-built every time you configure a new build directory, which could
be a non-trivial cost for some projects. It also may result in issues with large, complex projects
with many git submodules that each individually may use Corrosion. This can unnecessarily exacerbate
diamond dependency problems that wouldn't otherwise occur using an externally installed Corrosion.
On CMake >= 3.19 installing Corrosion does not offer any advantages, unless the native
tooling option is explicitly enabled.

### Package Manager

Coming soon...

### CMake Install

First, download and install Corrosion:
```bash
git clone https://github.com/corrosion-rs/corrosion.git
# Optionally, specify -DCMAKE_INSTALL_PREFIX=<target-install-path>. You can install Corrosion anyway
cmake -Scorrosion -Bbuild -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
# This next step may require sudo or admin privileges if you're installing to a system location,
# which is the default.
cmake --install build --config Release
```

You'll want to ensure that the install directory is available in your `PATH` or `CMAKE_PREFIX_PATH`
environment variable. This is likely to already be the case by default on a Unix system, but on
Windows it will install to `C:\Program Files (x86)\Corrosion` by default, which will not be in your
`PATH` or `CMAKE_PREFIX_PATH` by default.

Once Corrosion is installed and you've ensured the package is available in your `PATH`, you
can use it from your own project like any other package from your CMakeLists.txt:
```cmake
find_package(Corrosion REQUIRED)
```

### FetchContent
If you are using CMake >= 3.19 or installation is difficult or not feasible in
your environment, you can use the
[FetchContent](https://cmake.org/cmake/help/latest/module/FetchContent.html) module to include
Corrosion. This will download Corrosion and use it as if it were a subdirectory at configure time.

In your CMakeLists.txt:
```cmake
include(FetchContent)

FetchContent_Declare(
    Corrosion
    GIT_REPOSITORY https://github.com/corrosion-rs/corrosion.git
    GIT_TAG v0.3.0 # Optionally specify a commit hash, version tag or branch here
)
# Set any global configuration variables such as `Rust_TOOLCHAIN` before this line!
FetchContent_MakeAvailable(Corrosion)
```

### Subdirectory
Corrosion can also be used directly as a subdirectory. This solution may work well for small
projects, but it's discouraged for large projects with many dependencies, especially those which may
themselves use Corrosion. Either copy the Corrosion library into your source tree, being sure to
preserve the `LICENSE` file, or add this repository as a git submodule:
```bash
git submodule add https://github.com/corrosion-rs/corrosion.git
```

From there, using Corrosion is easy. In your CMakeLists.txt:
```cmake
add_subdirectory(path/to/corrosion)
```

## Usage
### Corrosion Options
All of the following variables are evaluated automatically in most cases. In typical cases you
shouldn't need to alter any of these. If you do want to specify them manually, make sure to set
them **before** `find_package(Corrosion REQUIRED)`.

- `Rust_TOOLCHAIN:STRING` - Specify a named rustup toolchain to use. Changes to this variable
resets all other options. Default: If the first-found `rustc` is a `rustup` proxy, then the default
rustup toolchain (see `rustup show`) is used. Otherwise, the variable is unset by default.
- `Rust_ROOT:STRING` - CMake provided. Path to a Rust toolchain to use. This is an alternative if
you want to select a specific Rust toolchain, but it's not managed by rustup. Default: Nothing
- `Rust_COMPILER:STRING` - Path to an actual `rustc`. If set to a `rustup` proxy, it will be
replaced by a path to an actual `rustc`. Default: The `rustc` in the first-found toolchain, either
from `rustup`, or from a toolchain available in the user's `PATH`.
- `Rust_CARGO:STRING` - Path to an actual `cargo`. Default: the `cargo` installed next to
`${Rust_COMPILER}`.
- `Rust_CARGO_TARGET:STRING` - The default target triple to build for. Alter for cross-compiling.
Default: On Visual Studio Generator, the matching triple for `CMAKE_VS_PLATFORM_NAME`. Otherwise,
the default target triple reported by `${Rust_COMPILER} --version --verbose`.
- `CORROSION_NATIVE_TOOLING:BOOL` - Use a native tool (written in Rust) as part of Corrosion. This
  option increases the configure-time significantly unless Corrosion is installed.
  Default: `OFF` if CMake >= 3.19.0. Forced `ON` for CMake < 3.19.

#### Developer/Maintainer Options
These options are not used in the course of normal Corrosion usage, but are used to configure how
Corrosion is built and installed. Only applies to Corrosion builds and subdirectory uses.

- `CORROSION_DEV_MODE:BOOL` - Indicates that Corrosion is being actively developed. Default: `OFF`
  if Corrosion is a subdirectory, `ON` if it is the top-level project
- `CORROSION_BUILD_TESTS:BOOL` - Build the Corrosion tests. Default: `Off` if Corrosion is a
  subdirectory, `ON` if it is the top-level project
- `CORROSION_GENERATOR_EXECUTABLE:STRING` - Specify a path to the corrosion-generator executable.
  This is to support scenarios where it's easier to build corrosion-generator outside of the normal
  bootstrap path, such as in the case of package managers that make it very easy to import Rust
  crates for fully reproducible, offline builds.
- `CORROSION_INSTALL_EXECUTABLE:BOOL` - Controls whether corrosion-generator is installed with the
  package. Default: `ON` with `CORROSION_GENERATOR_EXECUTABLE` unset, otherwise `OFF`

### Information provided by Corrosion

For your convenience, Corrosion sets a number of variables which contain information about the version of the rust
toolchain. You can use the CMake version comparison operators
(e.g. [`VERSION_GREATER_EQUAL`](https://cmake.org/cmake/help/latest/command/if.html#version-comparisons)) on the main
variable (e.g. `if(Rust_VERSION VERSION_GREATER_EQUAL "1.57.0")`), or you can inspect the major, minor and patch
versions individually.
- `Rust_VERSION<_MAJOR|_MINOR|_PATCH>` - The version of rustc.
- `Rust_CARGO_VERSION<_MAJOR|_MINOR|_PATCH>` - The cargo version.
- `Rust_LLVM_VERSION<_MAJOR|_MINOR|_PATCH>` - The LLVM version used by rustc.
- `Rust_IS_NIGHTLY` - 1 if a nightly toolchain is used, otherwise 0. Useful for selecting an unstable feature for a
  crate, that is only available on nightly toolchains.

### Adding crate targets

In order to integrate a Rust crate into CMake, you first need to import a crate or Workspace:
```cmake
corrosion_import_crate(MANIFEST_PATH <path/to/cargo.toml>
        # Equivalent to --all-features passed to cargo build
        [ALL_FEATURES]
        # Equivalent to --no-default-features passed to cargo build
        [NO_DEFAULT_FEATURES]
        # Disable linking of standard libraries (required for no_std crates).
        [NO_STD]
        # Specify  cargo build profile (e.g. release or a custom profile)
        [PROFILE <cargo-profile>]
        # Only import the specified crates from a workspace
        [CRATES <crate1> ... <crateN>]
        # Enable the specified features
        [FEATURES <feature1> ... <featureN>]
        # Pass additional arguments to `cargo build`
        [FLAGS <flag1> ... <flagN>]
)
```

This will add a cmake target for each imported crate. Many of the options can also be set per
target, see [Per Target options](#per-target-options) for details.

### Per Target options

Some configuration options can be specified individually for each target. You can set them via the
`corrosion_set_xxx()` functions specified below:

- `corrosion_set_env_vars(<target_name> <key1=value1> [... <keyN=valueN>])`: Define environment variables
  that should be set during the invocation of `cargo build` for the specified target. Please note that
  the environment variable will only be set for direct builds of the target via cmake, and not for any
  build where cargo built the crate in question as a dependency for another target.
  The environment variables may contain generator expressions.
- `corrosion_add_target_rustflags(<target_name> <rustflag> [... <rustflagN>])`: When building the target,
  the `RUSTFLAGS` environment variable will contain the flags added via this function. Please note that any
  dependencies (built by cargo) will also see these flags. In the future corrosion may offer a second function
  to allow specifying flags only for the target in question, utilizing `cargo rustc` instead of `cargo build`.
- `corrosion_add_target_local_rustflags(target_name rustc_flag [more_flags ...])`: Support setting
  rustflags for only the main target (crate ) and none of it's dependencies.
  This is useful in cases where you only need rustflags on the main-crate, but need to set different
  flags for different targets. Without "local" Rustflags this would require rebuilds of the
  dependencies when switching targets.
- `corrosion_set_hostbuild(<target_name>)`: The target should be compiled for the Host target and ignore any
  cross-compile configuration.
- `corrosion_set_features(<target_name> [ALL_FEATURES <Bool>] [NO_DEFAULT_FEATURES] [FEATURES <feature1> ... ])`:
  For a given target, enable specific features via `FEATURES`, toggle `ALL_FEATURES` on or off or disable all features
  via `NO_DEFAULT_FEATURES`. For more information on features, please see also the
  [cargo reference](https://doc.rust-lang.org/cargo/reference/features.html).
- `corrosion_set_flags(<target_name> <flag1> ...])`:
  For a given target, add options and flags at the end of `cargo build` invocation. This will be appended after any
  arguments passed through the `FLAGS` during the crate import.
- `corrosion_set_linker(target_name linker)`: Use `linker` to link the target.
  Please note that this only has an effect for targets where the final linker invocation is done
  by cargo, i.e. targets where foreign code is linked into rust code and not the other way around.
  Please also note that if you are cross-compiling and specify a linker such as `clang`, you are
  responsible for also adding a rustflag which adds the necessary `--target=` argument for the
  linker.

### Selecting a custom cargo profile

[Rust 1.57](https://blog.rust-lang.org/2021/12/02/Rust-1.57.0.html) stabilized the support for custom
[profiles](https://doc.rust-lang.org/cargo/reference/profiles.html). If you are using a sufficiently new rust toolchain,
you may select a custom profile by adding the optional argument `PROFILE <profile_name>` to
`corrosion_import_crate()`. If you do not specify a profile, or you use an older toolchain, corrosion will select
the standard `dev` profile if the CMake config is either `Debug` or unspecified. In all other cases the `release`
profile is chosen for cargo.

### Importing C-Style Libraries Written in Rust
Corrosion makes it completely trivial to import a crate into an existing CMake project. Consider
a project called [rust2cpp](test/rust2cpp/rust2cpp) with the following file structure:
```
rust2cpp/
    rust/
        src/
            lib.rs
        Cargo.lock
        Cargo.toml
    CMakeLists.txt
    main.cpp
```

This project defines a simple Rust lib crate, like so, in [`rust2cpp/rust/Cargo.toml`](test/rust2cpp/rust2cpp/rust/Cargo.toml):
```toml
[package]
name = "rust-lib"
version = "0.1.0"
authors = ["Andrew Gaspar <andrew.gaspar@outlook.com>"]
license = "MIT"
edition = "2018"

[dependencies]

[lib]
crate-type=["staticlib"]
```

In addition to `"staticlib"`, you can also use `"cdylib"`. In fact, you can define both with a
single crate and switch between which is used using the standard
[`BUILD_SHARED_LIBS`](https://cmake.org/cmake/help/latest/variable/BUILD_SHARED_LIBS.html) variable.

This crate defines a simple crate called `rust-lib`. Importing this crate into your
[CMakeLists.txt](test/rust2cpp/CMakeLists.txt) is trivial:
```cmake
# Note: you must have already included Corrosion for `corrosion_import_crate` to be available. See # the `Installation` section above.

corrosion_import_crate(MANIFEST_PATH rust/Cargo.toml)
```

Now that you've imported the crate into CMake, all of the executables, static libraries, and dynamic
libraries defined in the Rust can be directly referenced. So, merely define your C++ executable as
normal in CMake and add your crate's library using target_link_libraries:
```cmake
add_executable(cpp-exe main.cpp)
target_link_libraries(cpp-exe PUBLIC rust-lib)
```

That's it! You're now linking your Rust library to your C++ library.

#### Generate Bindings to Rust Library Automatically

Currently, you must manually declare bindings in your C or C++ program to the exported routines and
types in your Rust project. You can see boths sides of this in
[the Rust code](test/rust2cpp/rust/src/lib.rs) and in [the C++ code](test/rust2cpp/main.cpp).

Integration with [cbindgen](https://github.com/eqrion/cbindgen) is
planned for the future.

### Importing Libraries Written in C and C++ Into Rust

The rust targets can be imported with `corrosion_import_crate()` into CMake.
For targets where the linker should be invoked by Rust corrosion provides
`corrosion_link_libraries()` to link your C/C++ libraries with the Rust target.
For additional linker flags you may use `corrosion_add_target_local_rustflags()`
and pass linker arguments via the `-Clink-args` flag to rustc. These flags will
only be passed to the final rustc invocation and not affect any rust dependencies.

C bindings can be generated via [bindgen](https://github.com/rust-lang/rust-bindgen).
Corrosion does not offer any direct integration yet, but you can either generate the
bindings in the build-script of your crate, or generate the bindings as a CMake build step
(e.g. a custom target) and add a dependency from `cargo-prebuild_<rust_target>` to your
custom target for generating the bindings.

Example:

```cmake
# Import your Rust targets
corrosion_import_crate(MANIFEST_PATH rust/Cargo.toml)
# Link C/C++ libraries with your Rust target
corrosion_link_libraries(target_name c_library)
# Optionally explicitly define which linker to use.
corrosion_set_linker(target_name your_custom_linker)
# Optionally set linker arguments
corrosion_add_target_local_rustflags(target_name "-Clink-args=<linker arguments>")
# Optionally tell CMake that the rust crate depends on another target (e.g. a code generator)
add_dependencies(cargo-prebuild_<target_name> custom_bindings_target)
```

### Cross Compiling
Corrosion attempts to support cross-compiling as generally as possible, though not all
configurations are tested. Cross-compiling is explicitly supported in the following scenarios.

In all cases, you will need to install the standard library for the Rust target triple. When using
Rustup, you can use it to install the target standard library:

```bash
rustup target add <target-rust-triple>
```

If the target triple is automatically derived, Corrosion will print the target during configuration.
For example:

```
-- Rust Target: aarch64-linux-android
```

#### Windows-to-Windows
Corrosion supports cross-compiling between arbitrary Windows architectures using the Visual Studio
Generator. For example, to cross-compile for ARM64 from any platform, simply set the `-A`
architecture flag:

```bash
cmake -S. -Bbuild-arm64 -A ARM64
cmake --build build-arm64
```

Please note that for projects containing a build-script at least Rust 1.54 is required due to a bug
in previous cargo versions, which causes the build-script to incorrectly be built for the target
platform.

#### Linux-to-Linux
In order to cross-compile on Linux, you will need to install a cross-compiler. For example, on
Ubuntu, to cross compile for 64-bit Little-Endian PowerPC Little-Endian, install
`g++-powerpc64le-linux-gnu` from apt-get:

```bash
sudo apt install g++-powerpc64le-linux-gnu
```

Currently, Corrosion does not automatically determine the target triple while cross-compiling on
Linux, so you'll need to specify a matching `Rust_CARGO_TARGET`.

```bash
cmake -S. -Bbuild-ppc64le -DRust_CARGO_TARGET=powerpc64le-unknown-linux-gnu -DCMAKE_CXX_COMPILER=powerpc64le-linux-gnu-g++
cmake --build build-ppc64le
```

#### Android

Cross-compiling for Android is supported on all platforms with the Makefile and Ninja generators,
and the Rust target triple will automatically be selected. The CMake
[cross-compiling instructions for Android](https://cmake.org/cmake/help/latest/manual/cmake-toolchains.7.html#cross-compiling-for-android)
apply here. For example, to build for ARM64:

```bash
cmake -S. -Bbuild-android-arm64 -GNinja -DCMAKE_SYSTEM_NAME=Android \
      -DCMAKE_ANDROID_NDK=/path/to/android-ndk-rxxd -DCMAKE_ANDROID_ARCH_ABI=arm64-v8a
```

**Important note:** The Android SDK ships with CMake 3.10 at newest, which Android Studio will
prefer over any CMake you've installed locally. CMake 3.10 is insufficient for using Corrosion,
which requires a minimum of CMake 3.15. If you're using Android Studio to build your project,
follow the instructions in the Android Studio documentation for
[using a specific version of CMake](https://developer.android.com/studio/projects/install-ndk#vanilla_cmake).

## Limitations

### CMake `OUTPUT_DIRECTORY` target properties and `IMPORTED_LOCATION`

Corrosion respects the following `OUTPUT_DIRECTORY` target properties on CMake >= 3.19:
-   [ARCHIVE_OUTPUT_DIRECTORY](https://cmake.org/cmake/help/latest/prop_tgt/ARCHIVE_OUTPUT_DIRECTORY.html)
-   [LIBRARY_OUTPUT_DIRECTORY](https://cmake.org/cmake/help/latest/prop_tgt/LIBRARY_OUTPUT_DIRECTORY.html)
-   [RUNTIME_OUTPUT_DIRECTORY](https://cmake.org/cmake/help/latest/prop_tgt/RUNTIME_OUTPUT_DIRECTORY.html)
-   [PDB_OUTPUT_DIRECTORY](https://cmake.org/cmake/help/latest/prop_tgt/PDB_OUTPUT_DIRECTORY.html)

Due to limitations in CMake these target properties are evaluated in a deferred manner, to 
support the user setting the target properties after the call to `corrosion_import_crate()`.
This has the side effect that `IMPORTED_LOCATION` will be set late, and users should not
use `get_property` to read `IMPORTED_LOCATION` at configure time. Instead, generator expressions
should be used to get the location of the target artifact.
If `IMPORTED_LOCATION` is needed at configure time users may use `cmake_language(DEFER CALL ...)` to defer
evaluation to after the `IMPORTED_LOCATION` property is set.