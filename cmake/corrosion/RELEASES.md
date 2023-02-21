# 0.3.0 (2022-10-31)

## Breaking

- The minimum supported rust version (MSRV) was increased to 1.46, due to a cargo issue that recently
  surfaced on CI when using crates.io. On MacOS 12 and Windows-2022 at least Rust 1.54 is required.
- MacOS 10 and 11 are no longer officially supported and untested in CI.
- The minimum required CMake version is now 3.15.
- Adding a `PRE_BUILD` custom command on a `cargo-build_<target_name>` CMake target will no 
  longer work as expected. To support executing user defined commands before cargo build is
  invoked users should use the newly added targets `cargo-prebuild` (before all cargo build invocations)
  or `cargo-prebuild_<target_name>` as a dependency target. 
  Example: `add_dependencies(cargo-prebuild code_generator_target)`

### Breaking: Removed previously deprecated functionality
- Removed `add_crate()` function. Use `corrosio_import_crate()` instead.
- Removed `cargo_link_libraries()` function. Use `corrosion_link_libraries()` instead.
- Removed experimental CMake option `CORROSION_EXPERIMENTAL_PARSER`.
  The corresponding stable option is `CORROSION_NATIVE_TOOLING` albeit with inverted semantics.
- Previously Corrosion would set the `HOST_CC` and `HOST_CXX` environment variables when invoking 
  cargo build, if the environment variables `CC` and `CXX` outside of CMake where set.
  However this did not work as expected in all cases and sometimes the `HOST_CC` variable would be set
  to a cross-compiler for unknown reasons. For this reason `HOST_CC` and `HOST_CXX` are not set by
  corrosion anymore, but users can still set them manually if required via `corrosion_set_env_vars()`.
- The `CARGO_RUST_FLAGS` family of cache variables were removed. Corrosion does not internally use them
  anymore.

## Potentially breaking

- The working directory when invoking `cargo build` was changed to the directory of the Manifest
  file. This now allows cargo to pick up `.cargo/config.toml` files located in the source tree.
  ([205](https://github.com/corrosion-rs/corrosion/pull/205))
- Corrosion internally invokes `cargo build`. When passing arguments to `cargo build`, Corrosion
  now uses the CMake `VERBATIM` option. In rare cases this may require you to change how you quote
  parameters passed to corrosion (e.g. via `corrosion_add_target_rustflags()`).
  For example setting a `cfg` option previously required double escaping the rustflag like this
  `"--cfg=something=\\\"value\\\""`, but now it can be passed to corrosion without any escapes:
  `--cfg=something="value"`.
- Corrosion now respects the CMake `OUTPUT_DIRECTORY` target properties. More details in the "New features" section.

## New features

- Support setting rustflags for only the main target and none of its dependencies ([215](https://github.com/corrosion-rs/corrosion/pull/215)).
  A new function `corrosion_add_target_local_rustflags(target_name rustc_flag [more_flags ...])`
  is added for this purpose.
  This is useful in cases where you only need rustflags on the main-crate, but need to set different
  flags for different targets. Without "local" Rustflags this would require rebuilds of the
  dependencies when switching targets.
- Support explicitly selecting a linker ([208](https://github.com/corrosion-rs/corrosion/pull/208)).
  The linker can be selected via `corrosion_set_linker(target_name linker)`.
  Please note that this only has an effect for targets, where the final linker invocation is done
  by cargo, i.e. targets where foreign code is linked into rust code and not the other way around.
- Corrosion now respects the CMake `OUTPUT_DIRECTORY` target properties and copies build artifacts to the expected
  locations ([217](https://github.com/corrosion-rs/corrosion/pull/217)), if the properties are set.
  This feature requires at least CMake 3.19 and is enabled by default if supported. Please note that the `OUTPUT_NAME`
  target properties are currently not supported.
  Specifically, the following target properties are now respected:
  -   [ARCHIVE_OUTPUT_DIRECTORY](https://cmake.org/cmake/help/latest/prop_tgt/ARCHIVE_OUTPUT_DIRECTORY.html)
  -   [LIBRARY_OUTPUT_DIRECTORY](https://cmake.org/cmake/help/latest/prop_tgt/LIBRARY_OUTPUT_DIRECTORY.html)
  -   [RUNTIME_OUTPUT_DIRECTORY](https://cmake.org/cmake/help/latest/prop_tgt/RUNTIME_OUTPUT_DIRECTORY.html)
  -   [PDB_OUTPUT_DIRECTORY](https://cmake.org/cmake/help/latest/prop_tgt/PDB_OUTPUT_DIRECTORY.html)
- Corrosion now supports packages with potentially multiple binaries (bins) and a library (lib) at the
  same time. The only requirement is that the names of all `bin`s and `lib`s in the whole project must be unique.
  Users can set the names in the `Cargo.toml` by adding `name = <unique_name>` in the `[[bin]]` and `[lib]` tables.
- FindRust now has improved support for the `VERSION` option of `find_package` and will now attempt to find a matching
  toolchain version. Previously it was only checked if the default toolchain matched to required version.
- For rustup managed toolchains a CMake error is issued with a helpful message if the required target for
  the selected toolchain is not installed.

## Fixes

- Fix a CMake developer Warning when a Multi-Config Generator and Rust executable targets
  ([#213](https://github.com/corrosion-rs/corrosion/pull/213)).
- FindRust now respects the `QUIET` option to `find_package()` in most cases.

## Deprecation notice

- Support for the MSVC Generators with CMake toolchains before 3.20 is deprecated and will be removed in the next
  release (v0.4). All other Multi-config Generators already require CMake 3.20.

## Internal Changes

- The CMake Generator written in Rust and `CorrosionGenerator.cmake` which are responsible for parsing 
  `cargo metadata` output to create corresponding CMake targets for all Rust targets now share most code.
  This greatly simplified the CMake generator written in Rust and makes it much easier maintaining and adding
  new features regardless of how `cargo metadata` is parsed.

# 0.2.2 (2022-09-01)

## Fixes

- Do not use C++17 in the tests (makes tests work with older C++ compilers) ([184](https://github.com/corrosion-rs/corrosion/pull/184))
- Fix finding cargo on NixOS ([192](https://github.com/corrosion-rs/corrosion/pull/192))
- Fix issue with Rustflags test when using a Build type other than Debug and Release ([203](https://github.com/corrosion-rs/corrosion/pull/203)).

# 0.2.1 (2022-05-07)

## Fixes

- Fix missing variables provided by corrosion, when corrosion is used as a subdirectory ([181](https://github.com/corrosion-rs/corrosion/pull/181)):
  Public [Variables](https://github.com/corrosion-rs/corrosion#information-provided-by-corrosion) set
  by Corrosion were not visible when using Corrosion as a subdirectory, due to the wrong scope of
  the variables. This was fixed by promoting the respective variables to Cache variables.

# 0.2.0 (2022-05-05)

## Breaking changes

- Removed the integrator build script ([#156](https://github.com/corrosion-rs/corrosion/pull/156)).
  The build script provided by corrosion (for rust code that links in foreign code) is no longer necessary,
  so users can just remove the dependency.

## Deprecations

- Direct usage of the following target properties has been deprecated. The names of the custom properties are
  no longer considered part of the public API and may change in the future. Instead, please use the functions
  provided by corrosion. Internally different property names are used depending on the CMake version.
  - `CORROSION_FEATURES`, `CORROSION_ALL_FEATURES`, `CORROSION_NO_DEFAULT_FEATURES`. Instead please use
    `corrosion_set_features()`. See the updated Readme for details.
  - `CORROSION_ENVIRONMENT_VARIABLES`. Please use `corrosion_set_env_vars()` instead.
  - `CORROSION_USE_HOST_BUILD`. Please use `corrosion_set_hostbuild()` instead.
- The Minimum CMake version will likely be increased for the next major release. At the very least we want to drop
  support for CMake 3.12, but requiring CMake 3.16 or even 3.18 is also on the table. If you are using a CMake version
  that would be no longer supported by corrosion, please comment on issue
  [#168](https://github.com/corrosion-rs/corrosion/issues/168), so that we can gauge the number of affected users.

## New features

- Add `NO_STD` option to `corrosion_import_crate` ([#154](https://github.com/corrosion-rs/corrosion/pull/154)).
- Remove the requirement of building the Rust based generator crate for CMake >= 3.19. This makes using corrosion as
  a subdirectory as fast as the installed version (since everything is done in CMake).
  ([#131](https://github.com/corrosion-rs/corrosion/pull/131), [#161](https://github.com/corrosion-rs/corrosion/pull/161))
  If you do choose to install Corrosion, then by default the old Generator is still compiled and installed, so you can
  fall back to using it in case you use multiple cmake versions on the same machine for different projects.

## Fixes

- Fix Corrosion on MacOS 11 and 12 ([#167](https://github.com/corrosion-rs/corrosion/pull/167) and
  [#164](https://github.com/corrosion-rs/corrosion/pull/164)).
- Improve robustness of parsing the LLVM version (exported in `Rust_LLVM_VERSION`). It now also works for
  Rust versions, where the LLVM version is reported as `MAJOR.MINOR`. ([#148](https://github.com/corrosion-rs/corrosion/pull/148))
- Fix a bug which occurred when Corrosion was added multiple times via `add_subdirectory()`
  ([#143](https://github.com/corrosion-rs/corrosion/pull/143)).
- Set `CC_<target_triple_undercore>` and `CXX_<target_triple_undercore>` environment variables for the invocation of
  `cargo build` to the compilers selected by CMake  (if any)
  ([#138](https://github.com/corrosion-rs/corrosion/pull/138) and [#161](https://github.com/corrosion-rs/corrosion/pull/161)).
  This should ensure that C dependencies built in cargo buildscripts via [cc-rs](https://github.com/alexcrichton/cc-rs)
  use the same compiler as CMake built dependencies. Users can override the compiler by specifying the higher
  priority environment variable variants with dashes instead of underscores (See cc-rs documentation for details).
- Fix Ninja-Multiconfig Generator support for CMake versions >= 3.20. Previous CMake versions are missing a feature,
  which prevents us from supporting the Ninja-Multiconfig generator. ([#137](https://github.com/corrosion-rs/corrosion/pull/137))


# 0.1.0 (2022-02-01)

This is the first release of corrosion after it was moved to the new corrosion-rs organization.
Since there are no previous releases, this is not a complete changelog but only lists changes since
September 2021.

## New features
- [Add --profile support for rust >= 1.57](https://github.com/corrosion-rs/corrosion/pull/130):
  Allows users to specify a custom cargo profile with
  `corrosion_import_crate(... PROFILE <profilename>)`.
- [Add support for specifying per-target Rustflags](https://github.com/corrosion-rs/corrosion/pull/127):
  Rustflags can be added via `corrosion_add_target_rustflags(<target_name> [rustflags1...])`
- [Add `Rust_IS_NIGHTLY` and `Rust_LLVM_VERSION` variables](https://github.com/corrosion-rs/corrosion/pull/123):
  This may be useful if you want to conditionally enabled features when using a nightly toolchain
  or a specific LLVM Version.
- [Let `FindRust` fail gracefully if rustc is not found](https://github.com/corrosion-rs/corrosion/pull/111):
  This allows using `FindRust` in a more general setting (without corrosion).
- [Add support for cargo feature selection](https://github.com/corrosion-rs/corrosion/pull/108):
  See the [README](https://github.com/corrosion-rs/corrosion#cargo-feature-selection) for details on
  how to select features.


## Fixes
- [Fix the cargo-clean target](https://github.com/corrosion-rs/corrosion/pull/129)
- [Fix #84: CorrosionConfig.cmake looks in wrong place for Corrosion::Generator when CMAKE_INSTALL_LIBEXEC is an absolute path](https://github.com/corrosion-rs/corrosion/pull/122/commits/6f29af3ac53917ca2e0638378371e715a18a532d)
- [Fix #116: (Option CORROSION_INSTALL_EXECUTABLE not working)](https://github.com/corrosion-rs/corrosion/commit/97d44018fac1b1a2a7c095288c628f5bbd9b3184)
- [Fix building on Windows with rust >= 1.57](https://github.com/corrosion-rs/corrosion/pull/120)

## Known issues:
- Corrosion is currently not working on macos-11 and newer. See issue [#104](https://github.com/corrosion-rs/corrosion/issues/104).
  Contributions are welcome.
