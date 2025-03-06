#[=======================================================================[.rst:
FindRust
--------

Find Rust

This module finds an installed rustc compiler and the cargo build tool. If Rust
is managed by rustup it determines the available toolchains and returns a
concrete Rust version, not a rustup proxy.

Imported from Corrosion https://github.com/corrosion-rs/corrosion/

Copyright (c) 2018 Andrew Gaspar

Licensed under the MIT license

#]=======================================================================]

cmake_minimum_required(VERSION 3.12)

# search for Cargo here and set up a bunch of cool flags and stuff
include(FindPackageHandleStandardArgs)

list(APPEND CMAKE_MESSAGE_CONTEXT "FindRust")

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
            set(COMPARISON_OPERATOR "VERSION_LESS_EQUAL")
        elseif(Rust_FIND_VERSION_RANGE_MAX STREQUAL "EXCLUDE")
            set(COMPARISON_OPERATOR "VERSION_LESS")
        else()
            message(FATAL_ERROR "Unexpected value in `<PackageName>_FIND_VERSION_RANGE_MAX`: "
                    "`${Rust_FIND_VERSION_RANGE_MAX}`.")
        endif()
        if(("${ACTUAL_VERSION}" VERSION_GREATER_EQUAL "${Rust_FIND_VERSION_RANGE_MIN}")
                AND
            ( "${ACTUAL_VERSION}" ${COMPARISON_OPERATOR} "${Rust_FIND_VERSION_RANGE_MAX}" )
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

function(_corrosion_strip_target_triple input_triple_or_path output_triple)
    # If the target_triple is a path to a custom target specification file, then strip everything
    # except the filename from `target_triple`.
    get_filename_component(target_triple_ext "${input_triple_or_path}" EXT)
    set(target_triple "${input_triple_or_path}")
    if(target_triple_ext)
        if(target_triple_ext STREQUAL ".json")
            get_filename_component(target_triple "${input_triple_or_path}"  NAME_WE)
        endif()
    endif()
    set(${output_triple} "${target_triple}" PARENT_SCOPE)
endfunction()

function(_corrosion_parse_target_triple target_triple out_arch out_vendor out_os out_env)
    _corrosion_strip_target_triple(${target_triple} target_triple)

    # The vendor part may be left out from the target triple, and since `env` is also optional,
    # we determine if vendor is present by matching against a list of known vendors.
    set(known_vendors
        "apple"
        "esp[a-z0-9]*" # espressif, e.g. riscv32imc-esp-espidf or xtensa-esp32s3-none-elf
        "fortanix"
        "kmc"
        "pc"
        "nintendo"
        "nvidia"
        "openwrt"
        "alpine"
        "chimera"
        "unikraft"
        "unknown"
        "uwp" # aarch64-uwp-windows-msvc
        "wrs" # e.g. aarch64-wrs-vxworks
        "sony"
        "sun"
        )
    # todo: allow users to add additional vendors to the list via a cmake variable.
    list(JOIN known_vendors "|" known_vendors_joined)
    # vendor is optional - We detect if vendor is present by matching against a known list of
    # vendors. The next field is the OS, which we assume to always be present, while the last field
    # is again optional and contains the environment.
    string(REGEX MATCH
        "^([a-z0-9_\.]+)-((${known_vendors_joined})-)?([a-z0-9_]+)(-([a-z0-9_]+))?$"
        whole_match
        "${target_triple}"
        )
    if((NOT whole_match) AND (NOT CORROSION_NO_WARN_PARSE_TARGET_TRIPLE_FAILED))
        message(WARNING "Failed to parse target-triple `${target_triple}`."
            "Corrosion determines some information about the output artifacts based on OS "
            "specified in the Rust target-triple.\n"
            "Currently this is relevant for windows and darwin (mac) targets, since file "
            "extensions differ.\n"
            "Note: If you are targeting a different OS you can suppress this warning by"
            " setting the CMake cache variable "
            "`CORROSION_NO_WARN_PARSE_TARGET_TRIPLE_FAILED`."
            "Please consider opening an issue on github if you you need to add a new vendor to the list."
            )
    endif()

    message(DEBUG "Parsed Target triple: arch: ${CMAKE_MATCH_1}, vendor: ${CMAKE_MATCH_3}, "
        "OS: ${CMAKE_MATCH_4}, env: ${CMAKE_MATCH_6}")

    set("${out_arch}" "${CMAKE_MATCH_1}" PARENT_SCOPE)
    set("${out_vendor}" "${CMAKE_MATCH_3}" PARENT_SCOPE)
    set("${out_os}" "${CMAKE_MATCH_4}" PARENT_SCOPE)
    set("${out_env}" "${CMAKE_MATCH_6}" PARENT_SCOPE)
endfunction()

function(_corrosion_determine_libs_new target_triple out_libs)
    set(package_dir "${CMAKE_BINARY_DIR}/corrosion/required_libs")
    # Cleanup on reconfigure to get a cleans state (in case we change something in the future)
    file(REMOVE_RECURSE "${package_dir}")
    file(MAKE_DIRECTORY "${package_dir}")
    set(manifest "[package]\nname = \"required_libs\"\nedition = \"2018\"\nversion = \"0.1.0\"\n")
    string(APPEND manifest "\n[lib]\ncrate-type=[\"staticlib\"]\npath = \"lib.rs\"\n")
    string(APPEND manifest "\n[workspace]\n")
    file(WRITE "${package_dir}/Cargo.toml" "${manifest}")
    file(WRITE "${package_dir}/lib.rs" "pub fn add(left: usize, right: usize) -> usize {left + right}\n")

    execute_process(
        COMMAND ${CMAKE_COMMAND} -E env
            "CARGO_BUILD_RUSTC=${Rust_COMPILER_CACHED}"
        ${Rust_CARGO_CACHED} rustc --verbose --color never --target=${target_triple} -- --print=native-static-libs
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/corrosion/required_libs"
        RESULT_VARIABLE cargo_build_result
        ERROR_VARIABLE cargo_build_error_message
    )
    if(cargo_build_result)
        message(DEBUG "Determining required native libraries - failed: ${cargo_build_result}.")
        message(TRACE "The cargo build error was: ${cargo_build_error_message}")
        message(DEBUG "Note: This is expected for Rust targets without std support")
        return()
    else()
        # The pattern starts with `native-static-libs:` and goes to the end of the line.
        if(cargo_build_error_message MATCHES "native-static-libs: ([^\r\n]+)\r?\n")
            string(REPLACE " " ";" "libs_list" "${CMAKE_MATCH_1}")
            set(stripped_lib_list "")

            set(was_last_framework OFF)
            foreach(lib ${libs_list})
                # merge -framework;lib -> "-framework lib" as CMake does de-duplication of link libraries, and -framework prefix is required
                if (lib STREQUAL "-framework")
                    set(was_last_framework ON)
                    continue()
                endif()
                if (was_last_framework)
                    list(APPEND stripped_lib_list "-framework ${lib}")
                    set(was_last_framework OFF)
                    continue()
                endif()
                # Strip leading `-l` (unix) and potential .lib suffix (windows)
                string(REGEX REPLACE "^-l" "" "stripped_lib" "${lib}")
                string(REGEX REPLACE "\.lib$" "" "stripped_lib" "${stripped_lib}")
                list(APPEND stripped_lib_list "${stripped_lib}")
            endforeach()
            set(libs_list "${stripped_lib_list}")
            # Special case `msvcrt` to link with the debug version in Debug mode.
            list(TRANSFORM libs_list REPLACE "^msvcrt$" "\$<\$<CONFIG:Debug>:msvcrtd>")
        else()
            message(DEBUG "Determining required native libraries - failed: Regex match failure.")
            message(DEBUG "`native-static-libs` not found in: `${cargo_build_error_message}`")
            return()
        endif()
    endif()
    set("${out_libs}" "${libs_list}" PARENT_SCOPE)
endfunction()

if (NOT "${Rust_TOOLCHAIN}" STREQUAL "$CACHE{Rust_TOOLCHAIN}")
    # Promote Rust_TOOLCHAIN to a cache variable if it is not already a cache variable
    set(Rust_TOOLCHAIN ${Rust_TOOLCHAIN} CACHE STRING "Requested rustup toolchain" FORCE)
endif()

set(_RESOLVE_RUSTUP_TOOLCHAINS_DESC "Indicates whether to descend into the toolchain pointed to by rustup")
set(Rust_RESOLVE_RUSTUP_TOOLCHAINS ON CACHE BOOL ${_RESOLVE_RUSTUP_TOOLCHAINS_DESC})

# This block checks to see if we're prioritizing a rustup-managed toolchain.
if (DEFINED Rust_TOOLCHAIN)
    # If the user specifies `Rust_TOOLCHAIN`, then look for `rustup` first, rather than `rustc`.
    find_program(Rust_RUSTUP rustup PATHS "$ENV{HOME}/.cargo/bin")
    if(NOT Rust_RUSTUP)
        if(NOT "${Rust_FIND_QUIETLY}")
            message(
                WARNING "CMake variable `Rust_TOOLCHAIN` specified, but `rustup` was not found. "
                "Ignoring toolchain and looking for a Rust toolchain not managed by rustup.")
        endif()
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
        # Get `rustup` next to the `rustc` proxy
        get_filename_component(_RUST_PROXIES_PATH "${_Rust_COMPILER_TEST}" DIRECTORY)
        find_program(Rust_RUSTUP rustup HINTS "${_RUST_PROXIES_PATH}" NO_DEFAULT_PATH)
    endif()

    unset(_Rust_COMPILER_TEST CACHE)
endif()

# At this point, the only thing we should have evaluated is a path to `rustup` _if that's what the
# best source for a Rust toolchain was determined to be_.
if (NOT Rust_RUSTUP)
    set(Rust_RESOLVE_RUSTUP_TOOLCHAINS OFF CACHE BOOL ${_RESOLVE_RUSTUP_TOOLCHAINS_DESC} FORCE)
endif()

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
# `rustup` and the user hasn't explicitly requested to override this behavior, then select either
# the default toolchain, or the requested toolchain Rust_TOOLCHAIN
if (Rust_RESOLVE_RUSTUP_TOOLCHAINS)
    execute_process(
        COMMAND
            "${Rust_RUSTUP}" toolchain list --verbose
        OUTPUT_VARIABLE _TOOLCHAINS_RAW
    )

    string(REPLACE "\n" ";" _TOOLCHAINS_RAW "${_TOOLCHAINS_RAW}")
    set(_DISCOVERED_TOOLCHAINS "")
    set(_DISCOVERED_TOOLCHAINS_RUSTC_PATH "")
    set(_DISCOVERED_TOOLCHAINS_CARGO_PATH "")
    set(_DISCOVERED_TOOLCHAINS_VERSION "")

    foreach(_TOOLCHAIN_RAW ${_TOOLCHAINS_RAW})
        # We're going to try to parse the output of `rustup toolchain list --verbose`.
        # We expect output like this:
        #   stable-random-toolchain-junk (parenthesized-random-stuff-like-active-or-default) /path/to/toolchain/blah/more-blah
        # In the following regex, we capture the toolchain name, any parenthesized stuff, and then the path.
        message(STATUS "Parsing toolchain: ${_TOOLCHAIN_RAW}")
        if (_TOOLCHAIN_RAW MATCHES "([^\t ]+)[\t ]*(\\(.*\\))?[\t ]*(.+)")
            set(_TOOLCHAIN "${CMAKE_MATCH_1}")
            set(_TOOLCHAIN_TYPE "${CMAKE_MATCH_2}")

            set(_TOOLCHAIN_PATH "${CMAKE_MATCH_3}")
            set(_TOOLCHAIN_${_TOOLCHAIN}_PATH "${CMAKE_MATCH_3}")

            if (_TOOLCHAIN_TYPE MATCHES "default")
                set(_TOOLCHAIN_DEFAULT "${_TOOLCHAIN}")
            endif()

            if (_TOOLCHAIN_TYPE MATCHES "override")
                set(_TOOLCHAIN_OVERRIDE "${_TOOLCHAIN}")
            endif()

            execute_process(
                    COMMAND
                    "${_TOOLCHAIN_PATH}/bin/rustc" --version
                    OUTPUT_VARIABLE _TOOLCHAIN_RAW_VERSION
            )
            if (_TOOLCHAIN_RAW_VERSION MATCHES "rustc ([0-9]+)\\.([0-9]+)\\.([0-9]+)(-nightly)?")
                list(APPEND _DISCOVERED_TOOLCHAINS "${_TOOLCHAIN}")
                list(APPEND _DISCOVERED_TOOLCHAINS_RUSTC_PATH "${_TOOLCHAIN_PATH}/bin/rustc")
                list(APPEND _DISCOVERED_TOOLCHAINS_VERSION "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}.${CMAKE_MATCH_3}")

                # We need this variable to determine the default toolchain, since `foreach(... IN ZIP_LISTS ...)`
                # requires CMake 3.17. As a workaround we define this variable to lookup the version when iterating
                # through the `_DISCOVERED_TOOLCHAINS` lists.
                set(_TOOLCHAIN_${_TOOLCHAIN}_VERSION "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}.${CMAKE_MATCH_3}")
                if(CMAKE_MATCH_4)
                    set(_TOOLCHAIN_${_TOOLCHAIN}_IS_NIGHTLY "TRUE")
                else()
                    set(_TOOLCHAIN_${_TOOLCHAIN}_IS_NIGHTLY "FALSE")
                endif()
                if(EXISTS "${_TOOLCHAIN_PATH}/bin/cargo")
                    list(APPEND _DISCOVERED_TOOLCHAINS_CARGO_PATH "${_TOOLCHAIN_PATH}/bin/cargo")
                else()
                    list(APPEND _DISCOVERED_TOOLCHAINS_CARGO_PATH "NOTFOUND")
                endif()
            else()
                message(AUTHOR_WARNING "Unexpected output from `rustc --version` for Toolchain `${_TOOLCHAIN}`: "
                        "`${_TOOLCHAIN_RAW_VERSION}`.\n"
                        "Ignoring this toolchain."
                )
            endif()
        else()
            message(AUTHOR_WARNING "Didn't recognize toolchain: ${_TOOLCHAIN_RAW}. Ignoring this toolchain.\n"
                "Rustup toolchain list output( `${Rust_RUSTUP} toolchain list --verbose`):\n"
                "${_TOOLCHAINS_RAW}"
            )
        endif()
    endforeach()

    # Expose a list of available rustup toolchains.
    list(LENGTH _DISCOVERED_TOOLCHAINS _toolchain_len)
    list(LENGTH _DISCOVERED_TOOLCHAINS_RUSTC_PATH _toolchain_rustc_len)
    list(LENGTH _DISCOVERED_TOOLCHAINS_CARGO_PATH _toolchain_cargo_len)
    list(LENGTH _DISCOVERED_TOOLCHAINS_VERSION _toolchain_version_len)
    if(NOT
        (_toolchain_len EQUAL _toolchain_rustc_len
            AND _toolchain_cargo_len EQUAL _toolchain_version_len
            AND _toolchain_len EQUAL _toolchain_cargo_len)
        )
        message(FATAL_ERROR "Internal error - list length mismatch."
            "List lengths: ${_toolchain_len} toolchains, ${_toolchain_rustc_len} rustc, ${_toolchain_cargo_len} cargo,"
            " ${_toolchain_version_len} version. The lengths should be the same."
        )
    endif()

    set(Rust_RUSTUP_TOOLCHAINS CACHE INTERNAL "List of available Rustup toolchains" "${_DISCOVERED_TOOLCHAINS}")
    set(Rust_RUSTUP_TOOLCHAINS_RUSTC_PATH
        CACHE INTERNAL
        "List of the rustc paths corresponding to the toolchain at the same index in `Rust_RUSTUP_TOOLCHAINS`."
        "${_DISCOVERED_TOOLCHAINS_RUSTC_PATH}"
    )
    set(Rust_RUSTUP_TOOLCHAINS_CARGO_PATH
        CACHE INTERNAL
        "List of the cargo paths corresponding to the toolchain at the same index in `Rust_RUSTUP_TOOLCHAINS`. \
        May also be `NOTFOUND` if the toolchain does not have a cargo executable."
        "${_DISCOVERED_TOOLCHAINS_CARGO_PATH}"
    )
    set(Rust_RUSTUP_TOOLCHAINS_VERSION
        CACHE INTERNAL
        "List of the rust toolchain version corresponding to the toolchain at the same index in \
        `Rust_RUSTUP_TOOLCHAINS`."
        "${_DISCOVERED_TOOLCHAINS_VERSION}"
    )

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

    set(_RUST_TOOLCHAIN_PATH "${_TOOLCHAIN_${_RUSTUP_TOOLCHAIN_FULL}_PATH}")
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
elseif (Rust_RUSTUP)
    get_filename_component(_RUST_TOOLCHAIN_PATH "${Rust_RUSTUP}" DIRECTORY)
    get_filename_component(_RUST_TOOLCHAIN_PATH "${_RUST_TOOLCHAIN_PATH}" DIRECTORY)
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

if (Rust_RESOLVE_RUSTUP_TOOLCHAINS)
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
    unset(_CARGO_ARCH)
    unset(_CARGO_ABI)
    if (WIN32)
        if (CMAKE_VS_PLATFORM_NAME)
            string(TOLOWER "${CMAKE_VS_PLATFORM_NAME}" LOWER_VS_PLATFORM_NAME)
            if ("${LOWER_VS_PLATFORM_NAME}" STREQUAL "win32")
                set(_CARGO_ARCH i686)
            elseif("${LOWER_VS_PLATFORM_NAME}" STREQUAL "x64")
                set(_CARGO_ARCH x86_64)
            elseif("${LOWER_VS_PLATFORM_NAME}" STREQUAL "arm64")
                set(_CARGO_ARCH aarch64)
            else()
                message(WARNING "VS Platform '${CMAKE_VS_PLATFORM_NAME}' not recognized")
            endif()
        endif()
        # Fallback path
        if(NOT DEFINED _CARGO_ARCH)
            # Possible values for windows when not cross-compiling taken from here:
            # https://learn.microsoft.com/en-us/windows/win32/winprog64/wow64-implementation-details
            # When cross-compiling the user is expected to supply the value, so we match more variants.
            if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(AMD64|amd64|x86_64)$")
                set(_CARGO_ARCH x86_64)
            elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(ARM64|arm64|aarch64)$")
                set(_CARGO_ARCH aarch64)
            elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(X86|x86|i686)$")
                set(_CARGO_ARCH i686)
            elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "i586")
                set(_CARGO_ARCH i586)
            elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "IA64")
                message(FATAL_ERROR "No rust target for Intel Itanium.")
            elseif(NOT "${CMAKE_SYSTEM_PROCESSOR}")
                message(WARNING "Failed to detect target architecture. Please set `CMAKE_SYSTEM_PROCESSOR`"
                    " to your target architecture or set `Rust_CARGO_TARGET` to your cargo target triple."
                )
            else()
                message(WARNING "Failed to detect target architecture. Please set "
                    "`Rust_CARGO_TARGET` to your cargo target triple."
                )
            endif()
        endif()

        set(_CARGO_VENDOR "pc-windows")

        # The MSVC Generators will always target the msvc ABI.
        # For other generators we check the compiler ID and compiler target (if present)
        # If no compiler is set and we are not cross-compiling then we just choose the
        # default rust host target.
        if(DEFINED MSVC
            OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC"
            OR "${CMAKE_C_COMPILER_ID}" STREQUAL "MSVC"
            OR "${CMAKE_CXX_COMPILER_TARGET}" MATCHES "-msvc$"
            OR "${CMAKE_C_COMPILER_TARGET}" MATCHES "-msvc$"
        )
            set(_CARGO_ABI msvc)
        elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU"
            OR "${CMAKE_C_COMPILER_ID}" STREQUAL "GNU"
            OR "${CMAKE_CXX_COMPILER_TARGET}" MATCHES "-gnu$"
            OR "${CMAKE_C_COMPILER_TARGET}" MATCHES "-gnu$"
            OR (NOT CMAKE_CROSSCOMPILING AND "${Rust_DEFAULT_HOST_TARGET}" MATCHES "-gnu$")
            )
            set(_CARGO_ABI gnu)
        elseif(NOT "${CMAKE_CROSSCOMPILING}" AND "${Rust_DEFAULT_HOST_TARGET}" MATCHES "-msvc$")
            # We first check if the gnu branch matches to ensure this fallback is only used
            # if no compiler is enabled.
            set(_CARGO_ABI msvc)
        else()
            message(WARNING "Could not determine the target ABI. Please specify `Rust_CARGO_TARGET` manually.")
        endif()

        if(DEFINED _CARGO_ARCH AND DEFINED _CARGO_VENDOR AND DEFINED _CARGO_ABI)
            set(Rust_CARGO_TARGET_CACHED "${_CARGO_ARCH}-${_CARGO_VENDOR}-${_CARGO_ABI}"
                CACHE STRING "Target triple")
        endif()
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
    endif()
    # Fallback to the default host target
    if(NOT Rust_CARGO_TARGET_CACHED)
        if(CMAKE_CROSSCOMPILING)
            message(WARNING "CMake is in cross-compiling mode, but the cargo target-triple could not be inferred."
                "Falling back to the default host target. Please consider manually setting `Rust_CARGO_TARGET`."
            )
        endif()
        set(Rust_CARGO_TARGET_CACHED "${Rust_DEFAULT_HOST_TARGET}" CACHE STRING "Target triple")
    endif()

    message(STATUS "Rust Target: ${Rust_CARGO_TARGET_CACHED}")
endif()

if(Rust_CARGO_TARGET_CACHED STREQUAL Rust_DEFAULT_HOST_TARGET)
    set(Rust_CROSSCOMPILING FALSE CACHE INTERNAL "Rust is configured for cross-compiling")
else()
    set(Rust_CROSSCOMPILING TRUE  CACHE INTERNAL "Rust is configured for cross-compiling")
endif()

_corrosion_parse_target_triple("${Rust_CARGO_TARGET_CACHED}" rust_arch rust_vendor rust_os rust_env)
_corrosion_parse_target_triple("${Rust_CARGO_HOST_TARGET_CACHED}" rust_host_arch rust_host_vendor rust_host_os rust_host_env)

set(Rust_CARGO_TARGET_ARCH "${rust_arch}" CACHE INTERNAL "Target architecture")
set(Rust_CARGO_TARGET_VENDOR "${rust_vendor}" CACHE INTERNAL "Target vendor")
set(Rust_CARGO_TARGET_OS "${rust_os}" CACHE INTERNAL "Target Operating System")
set(Rust_CARGO_TARGET_ENV "${rust_env}" CACHE INTERNAL "Target environment")

set(Rust_CARGO_HOST_ARCH "${rust_host_arch}" CACHE INTERNAL "Host architecture")
set(Rust_CARGO_HOST_VENDOR "${rust_host_vendor}" CACHE INTERNAL "Host vendor")
set(Rust_CARGO_HOST_OS "${rust_host_os}" CACHE INTERNAL "Host Operating System")
set(Rust_CARGO_HOST_ENV "${rust_host_env}" CACHE INTERNAL "Host environment")

if(NOT DEFINED CACHE{Rust_CARGO_TARGET_LINK_NATIVE_LIBS})
    message(STATUS "Determining required link libraries for target ${Rust_CARGO_TARGET_CACHED}")
    unset(required_native_libs)
    _corrosion_determine_libs_new("${Rust_CARGO_TARGET_CACHED}" required_native_libs)
    if(DEFINED required_native_libs)
        message(STATUS "Required static libs for target ${Rust_CARGO_TARGET_CACHED}: ${required_native_libs}" )
    endif()
    # In very recent corrosion versions it is possible to override the rust compiler version
    # per target, so to be totally correct we would need to determine the libraries for
    # every installed Rust version, that the user could choose from.
    # In practice there aren't likely going to be any major differences, so we just do it once
    # for the target and once for the host target (if cross-compiling).
    set(Rust_CARGO_TARGET_LINK_NATIVE_LIBS "${required_native_libs}" CACHE INTERNAL
            "Required native libraries when linking Rust static libraries")
endif()

if(Rust_CROSSCOMPILING AND NOT DEFINED CACHE{Rust_CARGO_HOST_TARGET_LINK_NATIVE_LIBS})
    message(STATUS "Determining required link libraries for target ${Rust_CARGO_HOST_TARGET_CACHED}")
    unset(host_libs)
    _corrosion_determine_libs_new("${Rust_CARGO_HOST_TARGET_CACHED}" host_libs)
    if(DEFINED host_libs)
        message(STATUS "Required static libs for host target ${Rust_CARGO_HOST_TARGET_CACHED}: ${host_libs}" )
    endif()
    set(Rust_CARGO_HOST_TARGET_LINK_NATIVE_LIBS "${host_libs}" CACHE INTERNAL
        "Required native libraries when linking Rust static libraries for the host target")
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

list(POP_BACK CMAKE_MESSAGE_CONTEXT)
