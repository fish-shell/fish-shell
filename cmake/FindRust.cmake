#[=======================================================================[.rst:
Imported from Corrosion https://github.com/corrosion-rs/corrosion/

Copyright (c) 2018 Andrew Gaspar

Licensed under the MIT license

However this is absolutely gutted and reduced to the bare minimum.
#]=======================================================================]
include(FindPackageHandleStandardArgs)

cmake_minimum_required(VERSION 3.12)

# List of user variables that will override any toolchain-provided setting
set(_Rust_USER_VARS Rust_COMPILER Rust_CARGO Rust_CARGO_TARGET)
foreach(_VAR ${_Rust_USER_VARS})
    if (DEFINED "${_VAR}")
        set(${_VAR}_CACHED "${${_VAR}}" CACHE INTERNAL "Internal cache of ${_VAR}")
    else()
        unset(${_VAR}_CACHED CACHE)
    endif()
endforeach()

if (NOT DEFINED Rust_CARGO_CACHED)
  find_program(Rust_CARGO_CACHED cargo PATHS "$ENV{HOME}/.cargo/bin")
endif()

if (NOT EXISTS "${Rust_CARGO_CACHED}")
  message(FATAL_ERROR "The cargo executable ${Rust_CARGO_CACHED} was not found. "
    "Consider setting `Rust_CARGO_CACHED` to the absolute path of `cargo`."
  )
endif()

if (NOT DEFINED Rust_COMPILER_CACHED)
  find_program(Rust_COMPILER_CACHED rustc PATHS "$ENV{HOME}/.cargo/bin")
endif()


if (NOT EXISTS "${Rust_COMPILER_CACHED}")
    message(FATAL_ERROR "The rustc executable ${Rust_COMPILER} was not found. "
        "Consider setting `Rust_COMPILER` to the absolute path of `rustc`."
    )
endif()

# Figure out the target by just using the host target.
# If you want to cross-compile, you'll have to set Rust_CARGO_TARGET
if(NOT Rust_CARGO_TARGET_CACHED)
  execute_process(
    COMMAND "${Rust_COMPILER_CACHED}" --version --verbose
    OUTPUT_VARIABLE _RUSTC_VERSION_RAW
    RESULT_VARIABLE _RUSTC_VERSION_RESULT
  )

  if(NOT ( "${_RUSTC_VERSION_RESULT}" EQUAL "0" ))
    message(FATAL_ERROR "Failed to get rustc version.\n"
      "${Rust_COMPILER} --version failed with error: `${_RUSTC_VERSION_RESULT}`")
  endif()

  if (_RUSTC_VERSION_RAW MATCHES "host: ([a-zA-Z0-9_\\-]*)\n")
    set(Rust_DEFAULT_HOST_TARGET "${CMAKE_MATCH_1}")
  else()
    message(FATAL_ERROR
      "Failed to parse rustc host target. `rustc --version --verbose` evaluated to:\n${_RUSTC_VERSION_RAW}"
    )
  endif()

  if(CMAKE_CROSSCOMPILING)
    message(FATAL_ERROR "CMake is in cross-compiling mode."
      "Manually set `Rust_CARGO_TARGET`."
    )
  endif()
  set(Rust_CARGO_TARGET_CACHED "${Rust_DEFAULT_HOST_TARGET}" CACHE STRING "Target triple")
endif()

# Set the input variables as non-cache variables so that the variables are available after
# `find_package`, even if the values were evaluated to defaults.
foreach(_VAR ${_Rust_USER_VARS})
    set(${_VAR} "${${_VAR}_CACHED}")
    # Ensure cached variables have type INTERNAL
    set(${_VAR}_CACHED "${${_VAR}_CACHED}" CACHE INTERNAL "Internal cache of ${_VAR}")
endforeach()

find_package_handle_standard_args(
    Rust
    REQUIRED_VARS Rust_COMPILER Rust_CARGO Rust_CARGO_TARGET
)

