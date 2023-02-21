#[=======================================================================[.rst:
FindRust
--------

Find Rust

This module finds an installed rustc compiler and the cargo build tool. If Rust
is managed by rustup it determines the available toolchains and returns a
concrete Rust version, not a rustup proxy.

#]=======================================================================]

cmake_minimum_required(VERSION 3.12)

# search for Cargo here and set up a bunch of cool flags and stuff
include(FindPackageHandleStandardArgs)

# Print error message and return.
macro(_findrust_failed)
    if("${Rust_FIND_REQUIRED}")
        message(FATAL_ERROR ${ARGN})
    elseif(NOT "${Rust_FIND_QUIETLY}")
        message(WARNING ${ARGN})
    endif()
    # Note: PARENT_SCOPE is the scope of the caller of the caller of this macro.
    set(Rust_FOUND "" PARENT_SCOPE)
    return()
endmacro()

# Checks if the actual version of a Rust toolchain matches the VERSION requirements specified in find_package.
function(_findrust_version_ok ACTUAL_VERSION OUT_IS_OK)
    if(DEFINED Rust_FIND_VERSION_RANGE)
        if(Rust_FIND_VERSION_RANGE_MAX STREQUAL "INCLUDE")
            set(COMPARSION_OPERATOR "VERSION_LESS_EQUAL")
        elseif(Rust_FIND_VERSION_RANGE_MAX STREQUAL "EXCLUDE")
            set(COMPARSION_OPERATOR "VERSION_LESS")
        else()
            message(FATAL_ERROR "Unexpected value in `<PackageName>_FIND_VERSION_RANGE_MAX`: "
                    "`${Rust_FIND_VERSION_RANGE_MAX}`.")
        endif()
        if(("${ACTUAL_VERSION}" VERSION_GREATER_EQUAL "${Rust_FIND_VERSION_RANGE_MIN}")
                AND
            ( "${ACTUAL_VERSION}" ${COMPARSION_OPERATOR} "${Rust_FIND_VERSION_RANGE_MAX}" )
        )
            set("${OUT_IS_OK}" TRUE PARENT_SCOPE)
        else()
            set("${OUT_IS_OK}" FALSE PARENT_SCOPE)
        endif()
    elseif(DEFINED Rust_FIND_VERSION)
        if(Rust_VERSION_EXACT)
            set(COMPARISON_OPERATOR VERSION_EQUAL)
        else()
            set(COMPARISON_OPERATOR VERSION_GREATER_EQUAL)
        endif()
        if(_TOOLCHAIN_${_TOOLCHAIN_SELECTED}_VERSION "${COMPARISON_OPERATOR}" Rust_FIND_VERSION)
            set("${OUT_IS_OK}" TRUE PARENT_SCOPE)
        else()
            set("${OUT_IS_OK}" FALSE PARENT_SCOPE)
        endif()
    else()
        # if no VERSION requirement was specified, the version is always okay.
        set("${OUT_IS_OK}" TRUE PARENT_SCOPE)
    endif()
endfunction()

if (NOT "${Rust_TOOLCHAIN}" STREQUAL "$CACHE{Rust_TOOLCHAIN}")
    # Promote Rust_TOOLCHAIN to a cache variable if it is not already a cache variable
    set(Rust_TOOLCHAIN ${Rust_TOOLCHAIN} CACHE STRING "Requested rustup toolchain" FORCE)
endif()

# This block checks to see if we're prioritizing a rustup-managed toolchain.
if (DEFINED Rust_TOOLCHAIN)
    # If the user specifies `Rust_TOOLCHAIN`, then look for `rustup` first, rather than `rustc`.
    find_program(Rust_RUSTUP rustup PATHS "$ENV{HOME}/.cargo/bin")
    if (NOT Rust_RUSTUP AND NOT "${Rust_FIND_QUIETLY}")
        message(
            WARNING "CMake variable `Rust_TOOLCHAIN` specified, but `rustup` was not found. "
            "Ignoring toolchain and looking for a Rust toolchain not managed by rustup.")
    else()
        set(_RESOLVE_RUSTUP_TOOLCHAINS ON)
    endif()
else()
    # If we aren't definitely using a rustup toolchain, look for rustc first - the user may have
    # a toolchain installed via a method other than rustup higher in the PATH, which should be
    # preferred. However, if the first-found rustc is a rustup proxy, then we'll revert to
    # finding the preferred toolchain via rustup.

    # Uses `Rust_COMPILER` to let user-specified `rustc` win. But we will still "override" the
    # user's setting if it is pointing to `rustup`. Default rustup install path is provided as a
    # backup if a toolchain cannot be found in the user's PATH.

    if (DEFINED Rust_COMPILER)
        set(_Rust_COMPILER_TEST "${Rust_COMPILER}")
        set(_USER_SPECIFIED_RUSTC ON)
        if(NOT (EXISTS "${_Rust_COMPILER_TEST}" AND NOT IS_DIRECTORY "${_Rust_COMPILER_TEST}"))
            set(_ERROR_MESSAGE "Rust_COMPILER was set to `${Rust_COMPILER}`, but this file does "
                "not exist."
            )
            _findrust_failed(${_ERROR_MESSAGE})
            return()
        endif()
    else()
        find_program(_Rust_COMPILER_TEST rustc PATHS "$ENV{HOME}/.cargo/bin")
        if(NOT EXISTS "${_Rust_COMPILER_TEST}")
            set(_ERROR_MESSAGE "`rustc` not found in PATH or `$ENV{HOME}/.cargo/bin`.\n"
                    "Hint: Check if `rustc` is in PATH or manually specify the location "
                    "by setting `Rust_COMPILER` to the path to `rustc`.")
            _findrust_failed(${_ERROR_MESSAGE})
        endif()
    endif()

    # Check if the discovered rustc is actually a "rustup" proxy.
    execute_process(
        COMMAND
            ${CMAKE_COMMAND} -E env
                RUSTUP_FORCE_ARG0=rustup
            "${_Rust_COMPILER_TEST}" --version
        OUTPUT_VARIABLE _RUSTC_VERSION_RAW
        ERROR_VARIABLE _RUSTC_VERSION_STDERR
        RESULT_VARIABLE _RUSTC_VERSION_RESULT
    )

    if(NOT (_RUSTC_VERSION_RESULT EQUAL "0"))
        _findrust_failed("`${_Rust_COMPILER_TEST} --version` failed with ${_RUSTC_VERSION_RESULT}\n"
            "rustc stderr:\n${_RUSTC_VERSION_STDERR}"
            )
    endif()

    if (_RUSTC_VERSION_RAW MATCHES "rustup [0-9\\.]+")
        if (_USER_SPECIFIED_RUSTC)
            message(
                WARNING "User-specified Rust_COMPILER pointed to rustup's rustc proxy. Corrosion's "
                "FindRust will always try to evaluate to an actual Rust toolchain, and so the "
                "user-specified Rust_COMPILER will be discarded in favor of the default "
                "rustup-managed toolchain."
            )

            unset(Rust_COMPILER)
            unset(Rust_COMPILER CACHE)
        endif()

        set(_RESOLVE_RUSTUP_TOOLCHAINS ON)

        # Get `rustup` next to the `rustc` proxy
        get_filename_component(_RUST_PROXIES_PATH "${_Rust_COMPILER_TEST}" DIRECTORY)
        find_program(Rust_RUSTUP rustup HINTS "${_RUST_PROXIES_PATH}" NO_DEFAULT_PATH)
    endif()

    unset(_Rust_COMPILER_TEST CACHE)
endif()

# At this point, the only thing we should have evaluated is a path to `rustup` _if that's what the
# best source for a Rust toolchain was determined to be_.

# List of user variables that will override any toolchain-provided setting
set(_Rust_USER_VARS Rust_COMPILER Rust_CARGO Rust_CARGO_TARGET Rust_CARGO_HOST_TARGET)
foreach(_VAR ${_Rust_USER_VARS})
    if (DEFINED "${_VAR}")
        set(${_VAR}_CACHED "${${_VAR}}" CACHE INTERNAL "Internal cache of ${_VAR}")
    else()
        unset(${_VAR}_CACHED CACHE)
    endif()
endforeach()

# Discover what toolchains are installed by rustup, if the discovered `rustc` is a proxy from
# `rustup`, then select either the default toolchain, or the requested toolchain Rust_TOOLCHAIN
if (_RESOLVE_RUSTUP_TOOLCHAINS)
    execute_process(
        COMMAND
            "${Rust_RUSTUP}" toolchain list --verbose
        OUTPUT_VARIABLE _TOOLCHAINS_RAW
    )

    string(REPLACE "\n" ";" _TOOLCHAINS_RAW "${_TOOLCHAINS_RAW}")

    foreach(_TOOLCHAIN_RAW ${_TOOLCHAINS_RAW})
        if (_TOOLCHAIN_RAW MATCHES "([a-zA-Z0-9\\._\\-]+)[ \t\r\n]?(\\(default\\) \\(override\\)|\\(default\\)|\\(override\\))?[ \t\r\n]+(.+)")
            set(_TOOLCHAIN "${CMAKE_MATCH_1}")
            set(_TOOLCHAIN_TYPE "${CMAKE_MATCH_2}")
            list(APPEND _DISCOVERED_TOOLCHAINS "${_TOOLCHAIN}")

            set(${_TOOLCHAIN}_PATH "${CMAKE_MATCH_3}")

            if (_TOOLCHAIN_TYPE MATCHES ".*\\(default\\).*")
                set(_TOOLCHAIN_DEFAULT "${_TOOLCHAIN}")
            endif()

            if (_TOOLCHAIN_TYPE MATCHES ".*\\(override\\).*")
                set(_TOOLCHAIN_OVERRIDE "${_TOOLCHAIN}")
            endif()

            execute_process(
                    COMMAND
                    "${_TOOLCHAIN}/bin/rustc" --version
                    OUTPUT_VARIABLE _TOOLCHAIN_RAW_VERSION
            )
            if (_TOOLCHAIN_RAW_VERSION MATCHES "rustc ([0-9]+)\\.([0-9]+)\\.([0-9]+)(-nightly)?")
                # todo: maybe needs to be advanced cache variable...
                set(_TOOLCHAIN_${_TOOLCHAIN}_VERSION "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}.${CMAKE_MATCH_3}")
                if(CMAKE_MATCH_4)
                    set(_TOOLCHAIN_${_TOOLCHAIN}_IS_NIGHTLY "TRUE")
                else()
                    set(_TOOLCHAIN_${_TOOLCHAIN}_IS_NIGHTLY "FALSE")
                endif()
            endif()

        else()
            message(WARNING "Didn't recognize toolchain: ${_TOOLCHAIN_RAW}")
        endif()
    endforeach()

    # Rust_TOOLCHAIN is preferred over a requested version if it is set.
    if (NOT DEFINED Rust_TOOLCHAIN)
        if (NOT DEFINED _TOOLCHAIN_OVERRIDE)
            set(_TOOLCHAIN_SELECTED "${_TOOLCHAIN_DEFAULT}")
        else()
            set(_TOOLCHAIN_SELECTED "${_TOOLCHAIN_OVERRIDE}")
        endif()
        # Check default toolchain first.
        _findrust_version_ok("_TOOLCHAIN_${_TOOLCHAIN_SELECTED}_VERSION" _VERSION_OK)
        if(NOT "${_VERSION_OK}")
            foreach(_TOOLCHAIN "${_DISCOVERED_TOOLCHAINS}")
                _findrust_version_ok("_TOOLCHAIN_${_TOOLCHAIN}_VERSION" _VERSION_OK)
                if("${_VERSION_OK}")
                    set(_TOOLCHAIN_SELECTED "${_TOOLCHAIN}")
                    break()
                endif()
            endforeach()
            # Check if we found a suitable version in the for loop.
            if(NOT "${_VERSION_OK}")
                string(REPLACE ";" "\n" _DISCOVERED_TOOLCHAINS "${_DISCOVERED_TOOLCHAINS}")
                _findrust_failed("Failed to find a Rust toolchain matching the version requirements of "
                        "${Rust_FIND_VERSION}. Available toolchains: ${_DISCOVERED_TOOLCHAINS}")
            endif()
        endif()
    endif()

    set(Rust_TOOLCHAIN "${_TOOLCHAIN_SELECTED}" CACHE STRING "The rustup toolchain to use")
    set_property(CACHE Rust_TOOLCHAIN PROPERTY STRINGS "${_DISCOVERED_TOOLCHAINS}")

    if(NOT Rust_FIND_QUIETLY)
        message(STATUS "Rust Toolchain: ${Rust_TOOLCHAIN}")
    endif()

    if (NOT Rust_TOOLCHAIN IN_LIST _DISCOVERED_TOOLCHAINS)
        # If the precise toolchain wasn't found, try appending the default host
        execute_process(
            COMMAND
                "${Rust_RUSTUP}" show
            RESULT_VARIABLE _SHOW_RESULT
            OUTPUT_VARIABLE _SHOW_RAW
        )
        if(NOT "${_SHOW_RESULT}" EQUAL "0")
            _findrust_failed("Command `${Rust_RUSTUP} show` failed")
        endif()

        if (_SHOW_RAW MATCHES "Default host: ([a-zA-Z0-9_\\-]*)\n")
            set(_DEFAULT_HOST "${CMAKE_MATCH_1}")
        else()
            _findrust_failed("Failed to parse \"Default host\" from `${Rust_RUSTUP} show`. Got: ${_SHOW_RAW}")
        endif()

        if (NOT "${Rust_TOOLCHAIN}-${_DEFAULT_HOST}" IN_LIST _DISCOVERED_TOOLCHAINS)
            set(_NOT_FOUND_MESSAGE "Could not find toolchain '${Rust_TOOLCHAIN}'\n"
                "Available toolchains:\n"
            )
            foreach(_TOOLCHAIN ${_DISCOVERED_TOOLCHAINS})
                list(APPEND _NOT_FOUND_MESSAGE "  `${_TOOLCHAIN}`\n")
            endforeach()
            _findrust_failed(${_NOT_FOUND_MESSAGE})
        endif()

        set(_RUSTUP_TOOLCHAIN_FULL "${Rust_TOOLCHAIN}-${_DEFAULT_HOST}")
    else()
        set(_RUSTUP_TOOLCHAIN_FULL "${Rust_TOOLCHAIN}")
    endif()

    set(_RUST_TOOLCHAIN_PATH "${${_RUSTUP_TOOLCHAIN_FULL}_PATH}")
    if(NOT "${Rust_FIND_QUIETLY}")
        message(VERBOSE "Rust toolchain ${_RUSTUP_TOOLCHAIN_FULL}")
        message(VERBOSE "Rust toolchain path ${_RUST_TOOLCHAIN_PATH}")
    endif()

    # Is overridden if the user specifies `Rust_COMPILER` explicitly.
    find_program(
        Rust_COMPILER_CACHED
        rustc
            HINTS "${_RUST_TOOLCHAIN_PATH}/bin"
            NO_DEFAULT_PATH)
else()
    find_program(Rust_COMPILER_CACHED rustc)
    if (EXISTS "${Rust_COMPILER_CACHED}")
        # rustc is expected to be at `<toolchain_path>/bin/rustc`.
        get_filename_component(_RUST_TOOLCHAIN_PATH "${Rust_COMPILER_CACHED}" DIRECTORY)
        get_filename_component(_RUST_TOOLCHAIN_PATH "${_RUST_TOOLCHAIN_PATH}" DIRECTORY)
    endif()
endif()

if (NOT EXISTS "${Rust_COMPILER_CACHED}")
    set(_NOT_FOUND_MESSAGE "The rustc executable was not found. "
        "Rust not installed or ~/.cargo/bin not added to path?\n"
        "Hint: Consider setting `Rust_COMPILER` to the absolute path of `rustc`."
    )
    _findrust_failed(${_NOT_FOUND_MESSAGE})
endif()

if (_RESOLVE_RUSTUP_TOOLCHAINS)
    set(_NOT_FOUND_MESSAGE "Rust was detected to be managed by rustup, but failed to find `cargo` "
        "next to `rustc` in `${_RUST_TOOLCHAIN_PATH}/bin`. This can happen for custom toolchains, "
        "if cargo was not built. "
        "Please manually specify the path to a compatible `cargo` by setting `Rust_CARGO`."
    )
    find_program(
        Rust_CARGO_CACHED
        cargo
            HINTS "${_RUST_TOOLCHAIN_PATH}/bin"
            NO_DEFAULT_PATH
    )
    # note: maybe can use find_package_handle_standard_args here, if we remove the _CACHED postfix.
    # not sure why that is here...
    if(NOT EXISTS "${Rust_CARGO_CACHED}")
        _findrust_failed(${_NOT_FOUND_MESSAGE})
    endif()
    set(Rust_TOOLCHAIN_IS_RUSTUP_MANAGED TRUE CACHE INTERNAL "" FORCE)
else()
    set(_NOT_FOUND_MESSAGE "Failed to find `cargo` in PATH and `${_RUST_TOOLCHAIN_PATH}/bin`.\n"
        "Please ensure cargo is in PATH or manually specify the path to a compatible `cargo` by "
        "setting `Rust_CARGO`."
    )
    # On some systems (e.g. NixOS) cargo is not managed by rustup and also not next to rustc.
    find_program(
            Rust_CARGO_CACHED
            cargo
                HINTS "${_RUST_TOOLCHAIN_PATH}/bin"
    )
    # note: maybe can use find_package_handle_standard_args here, if we remove the _CACHED postfix.
    # not sure why that is here...
    if(NOT EXISTS "${Rust_CARGO_CACHED}")
        _findrust_failed(${_NOT_FOUND_MESSAGE})
    endif()
endif()

execute_process(
    COMMAND "${Rust_CARGO_CACHED}" --version --verbose
    OUTPUT_VARIABLE _CARGO_VERSION_RAW
    RESULT_VARIABLE _CARGO_VERSION_RESULT
)
# todo: check if cargo is a required component!
if(NOT ( "${_CARGO_VERSION_RESULT}" EQUAL "0" ))
    _findrust_failed("Failed to get cargo version.\n"
        "`${Rust_CARGO_CACHED} --version` failed with error: `${_CARGO_VERSION_RESULT}"
)
endif()

# todo: don't set cache variables here, but let find_package_handle_standard_args do the promotion
# later.
if (_CARGO_VERSION_RAW MATCHES "cargo ([0-9]+)\\.([0-9]+)\\.([0-9]+)")
    set(Rust_CARGO_VERSION_MAJOR "${CMAKE_MATCH_1}" CACHE INTERNAL "" FORCE)
    set(Rust_CARGO_VERSION_MINOR "${CMAKE_MATCH_2}" CACHE INTERNAL "" FORCE)
    set(Rust_CARGO_VERSION_PATCH "${CMAKE_MATCH_3}" CACHE INTERNAL "" FORCE)
    set(Rust_CARGO_VERSION "${Rust_CARGO_VERSION_MAJOR}.${Rust_CARGO_VERSION_MINOR}.${Rust_CARGO_VERSION_PATCH}" CACHE INTERNAL "" FORCE)
# Workaround for the version strings where the `cargo ` prefix is missing.
elseif(_CARGO_VERSION_RAW MATCHES "([0-9]+)\\.([0-9]+)\\.([0-9]+)")
    set(Rust_CARGO_VERSION_MAJOR "${CMAKE_MATCH_1}" CACHE INTERNAL "" FORCE)
    set(Rust_CARGO_VERSION_MINOR "${CMAKE_MATCH_2}" CACHE INTERNAL "" FORCE)
    set(Rust_CARGO_VERSION_PATCH "${CMAKE_MATCH_3}" CACHE INTERNAL "" FORCE)
    set(Rust_CARGO_VERSION "${Rust_CARGO_VERSION_MAJOR}.${Rust_CARGO_VERSION_MINOR}.${Rust_CARGO_VERSION_PATCH}" CACHE INTERNAL "" FORCE)
else()
    _findrust_failed(
        "Failed to parse cargo version. `cargo --version` evaluated to (${_CARGO_VERSION_RAW}). "
        "Expected a <Major>.<Minor>.<Patch> version triple."
    )
endif()

execute_process(
    COMMAND "${Rust_COMPILER_CACHED}" --version --verbose
    OUTPUT_VARIABLE _RUSTC_VERSION_RAW
    RESULT_VARIABLE _RUSTC_VERSION_RESULT
)

if(NOT ( "${_RUSTC_VERSION_RESULT}" EQUAL "0" ))
    _findrust_failed("Failed to get rustc version.\n"
        "${Rust_COMPILER_CACHED} --version failed with error: `${_RUSTC_VERSION_RESULT}`")
endif()

if (_RUSTC_VERSION_RAW MATCHES "rustc ([0-9]+)\\.([0-9]+)\\.([0-9]+)(-nightly)?")
    set(Rust_VERSION_MAJOR "${CMAKE_MATCH_1}" CACHE INTERNAL "" FORCE)
    set(Rust_VERSION_MINOR "${CMAKE_MATCH_2}" CACHE INTERNAL "" FORCE)
    set(Rust_VERSION_PATCH "${CMAKE_MATCH_3}" CACHE INTERNAL "" FORCE)
    set(Rust_VERSION "${Rust_VERSION_MAJOR}.${Rust_VERSION_MINOR}.${Rust_VERSION_PATCH}" CACHE INTERNAL "" FORCE)
    if(CMAKE_MATCH_4)
        set(Rust_IS_NIGHTLY 1 CACHE INTERNAL "" FORCE)
    else()
        set(Rust_IS_NIGHTLY 0 CACHE INTERNAL "" FORCE)
    endif()
else()
    _findrust_failed("Failed to parse rustc version. `${Rust_COMPILER_CACHED} --version --verbose` "
        "evaluated to:\n`${_RUSTC_VERSION_RAW}`"
    )
endif()

if (_RUSTC_VERSION_RAW MATCHES "host: ([a-zA-Z0-9_\\-]*)\n")
    set(Rust_DEFAULT_HOST_TARGET "${CMAKE_MATCH_1}")
    set(Rust_CARGO_HOST_TARGET_CACHED "${Rust_DEFAULT_HOST_TARGET}" CACHE STRING "Host triple")
else()
    _findrust_failed(
        "Failed to parse rustc host target. `rustc --version --verbose` evaluated to:\n${_RUSTC_VERSION_RAW}"
    )
endif()

if (_RUSTC_VERSION_RAW MATCHES "LLVM version: ([0-9]+)\\.([0-9]+)(\\.([0-9]+))?")
    set(Rust_LLVM_VERSION_MAJOR "${CMAKE_MATCH_1}" CACHE INTERNAL "" FORCE)
    set(Rust_LLVM_VERSION_MINOR "${CMAKE_MATCH_2}" CACHE INTERNAL "" FORCE)
    # With the Rust toolchain 1.44.1 the reported LLVM version is 9.0, i.e. without a patch version.
    # Since cmake regex does not support non-capturing groups, just ignore Match 3.
    set(Rust_LLVM_VERSION_PATCH "${CMAKE_MATCH_4}" CACHE INTERNAL "" FORCE)
    set(Rust_LLVM_VERSION "${Rust_LLVM_VERSION_MAJOR}.${Rust_LLVM_VERSION_MINOR}.${Rust_LLVM_VERSION_PATCH}" CACHE INTERNAL "" FORCE)
elseif(NOT Rust_FIND_QUIETLY)
    message(
            WARNING
            "Failed to parse rustc LLVM version. `rustc --version --verbose` evaluated to:\n${_RUSTC_VERSION_RAW}"
    )
endif()

if (NOT Rust_CARGO_TARGET_CACHED)
    if (WIN32)
        if (CMAKE_VS_PLATFORM_NAME)
            if ("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "Win32")
                set(_CARGO_ARCH i686)
            elseif("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
                set(_CARGO_ARCH x86_64)
            elseif("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "ARM64")
                set(_CARGO_ARCH aarch64)
            else()
                message(WARNING "VS Platform '${CMAKE_VS_PLATFORM_NAME}' not recognized")
            endif()
        else ()
            if (NOT DEFINED CMAKE_SIZEOF_VOID_P)
                # todo: this should only be best effort instead of failure
                message(
                    FATAL_ERROR "Compiler hasn't been enabled yet - can't determine the target architecture")
            endif()

            if (CMAKE_SIZEOF_VOID_P EQUAL 8)
                set(_CARGO_ARCH x86_64)
            else()
                set(_CARGO_ARCH i686)
            endif()
        endif()

        set(_CARGO_VENDOR "pc-windows")

        if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            set(_CARGO_ABI gnu)
        else()
            set(_CARGO_ABI msvc)
        endif()

        set(Rust_CARGO_TARGET_CACHED "${_CARGO_ARCH}-${_CARGO_VENDOR}-${_CARGO_ABI}"
            CACHE STRING "Target triple")
    elseif (ANDROID)
        if (CMAKE_ANDROID_ARCH_ABI STREQUAL armeabi-v7a)
            if (CMAKE_ANDROID_ARM_MODE)
                set(_Rust_ANDROID_TARGET armv7-linux-androideabi)
            else ()
                set(_Rust_ANDROID_TARGET thumbv7neon-linux-androideabi)
            endif()
        elseif (CMAKE_ANDROID_ARCH_ABI STREQUAL arm64-v8a)
            set(_Rust_ANDROID_TARGET aarch64-linux-android)
        elseif (CMAKE_ANDROID_ARCH_ABI STREQUAL x86)
            set(_Rust_ANDROID_TARGET i686-linux-android)
        elseif (CMAKE_ANDROID_ARCH_ABI STREQUAL x86_64)
            set(_Rust_ANDROID_TARGET x86_64-linux-android)
        endif()

        if (_Rust_ANDROID_TARGET)
            set(Rust_CARGO_TARGET_CACHED "${_Rust_ANDROID_TARGET}" CACHE STRING "Target triple")
        endif()
    else()
        set(Rust_CARGO_TARGET_CACHED "${Rust_DEFAULT_HOST_TARGET}" CACHE STRING "Target triple")
    endif()

    message(STATUS "Rust Target: ${Rust_CARGO_TARGET_CACHED}")
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
    REQUIRED_VARS Rust_COMPILER Rust_VERSION Rust_CARGO Rust_CARGO_VERSION Rust_CARGO_TARGET Rust_CARGO_HOST_TARGET
    VERSION_VAR Rust_VERSION
)


if(NOT TARGET Rust::Rustc)
    add_executable(Rust::Rustc IMPORTED GLOBAL)
    set_property(
        TARGET Rust::Rustc
        PROPERTY IMPORTED_LOCATION "${Rust_COMPILER_CACHED}"
    )

    add_executable(Rust::Cargo IMPORTED GLOBAL)
    set_property(
        TARGET Rust::Cargo
        PROPERTY IMPORTED_LOCATION "${Rust_CARGO_CACHED}"
    )
    set(Rust_FOUND true)
endif()
