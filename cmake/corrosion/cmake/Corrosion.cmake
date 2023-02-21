cmake_minimum_required(VERSION 3.15)

if (CMAKE_GENERATOR STREQUAL "Ninja Multi-Config" AND CMAKE_VERSION VERSION_LESS 3.20.0)
    message(FATAL_ERROR "Corrosion requires at least CMake 3.20 with the \"Ninja Multi-Config\" "
       "generator. Please use a different generator or update to cmake >= 3.20.")
elseif(CMAKE_VERSION VERSION_LESS 3.20.0 AND CMAKE_CONFIGURATION_TYPES)
    message(DEPRECATION "Corrosion will require at least CMake 3.20 for use with all Multi-Config"
            "Generators starting with Corrosion 0.4. Please consider upgrading your CMake version"
            " or using a different Generator."
    )
endif()

option(CORROSION_VERBOSE_OUTPUT "Enables verbose output from Corrosion and Cargo" OFF)

set(CORROSION_NATIVE_TOOLING_DESCRIPTION
    "Use native tooling - Required on CMake < 3.19 and available as a fallback option for recent versions"
    )

set(CORROSION_RESPECT_OUTPUT_DIRECTORY_DESCRIPTION
    "Respect the CMake target properties specifying the output directory of a target, such as
    `RUNTIME_OUTPUT_DIRECTORY`. This requires CMake >= 3.19, otherwise this option is forced off."
)

option(
    CORROSION_NATIVE_TOOLING
    "${CORROSION_NATIVE_TOOLING_DESCRIPTION}"
    OFF
)

option(CORROSION_RESPECT_OUTPUT_DIRECTORY
    "${CORROSION_RESPECT_OUTPUT_DIRECTORY_DESCRIPTION}"
    ON
)

option(
    CORROSION_NO_WARN_PARSE_TARGET_TRIPLE_FAILED
    "Surpresses a warning if the parsing the target triple failed."
    OFF
)

# The native tooling is required on CMAke < 3.19 so we override whatever the user may have set.
if (CMAKE_VERSION VERSION_LESS 3.19.0)
    set(CORROSION_NATIVE_TOOLING ON CACHE INTERNAL "${CORROSION_NATIVE_TOOLING_DESCRIPTION}" FORCE)
    set(CORROSION_RESPECT_OUTPUT_DIRECTORY OFF CACHE INTERNAL
        "${CORROSION_RESPECT_OUTPUT_DIRECTORY_DESCRIPTION}" FORCE
    )
endif()

find_package(Rust REQUIRED)

if(Rust_TOOLCHAIN_IS_RUSTUP_MANAGED)
    execute_process(COMMAND rustup target list --toolchain "${Rust_TOOLCHAIN}"
            OUTPUT_VARIABLE AVAILABLE_TARGETS_RAW
    )
    string(REPLACE "\n" ";" AVAILABLE_TARGETS_RAW "${AVAILABLE_TARGETS_RAW}")
    string(REPLACE " (installed)" "" "AVAILABLE_TARGETS" "${AVAILABLE_TARGETS_RAW}")
    set(INSTALLED_TARGETS_RAW "${AVAILABLE_TARGETS_RAW}")
    list(FILTER INSTALLED_TARGETS_RAW INCLUDE REGEX " \\(installed\\)")
    string(REPLACE " (installed)" "" "INSTALLED_TARGETS" "${INSTALLED_TARGETS_RAW}")
    list(TRANSFORM INSTALLED_TARGETS STRIP)
    if("${Rust_CARGO_TARGET}" IN_LIST AVAILABLE_TARGETS)
        message(DEBUG "Cargo target ${Rust_CARGO_TARGET} is an official target-triple")
        message(DEBUG "Installed targets: ${INSTALLED_TARGETS}")
        if(NOT ("${Rust_CARGO_TARGET}" IN_LIST INSTALLED_TARGETS))
            message(FATAL_ERROR "Target ${Rust_CARGO_TARGET} is not installed for toolchain ${Rust_TOOLCHAIN}.\n"
                    "Help: Run `rustup target add --toolchain ${Rust_TOOLCHAIN} ${Rust_CARGO_TARGET}` to install "
                    "the missing target."
            )
        endif()
    endif()

endif()

if(CMAKE_GENERATOR MATCHES "Visual Studio"
        AND (NOT CMAKE_VS_PLATFORM_NAME STREQUAL CMAKE_VS_PLATFORM_NAME_DEFAULT)
        AND Rust_VERSION VERSION_LESS "1.54")
    message(FATAL_ERROR "Due to a cargo issue, cross-compiling with a Visual Studio generator and rust versions"
            " before 1.54 is not supported. Rust build scripts would be linked with the cross-compiler linker, which"
            " causes the build to fail. Please upgrade your Rust version to 1.54 or newer.")
endif()

if (NOT TARGET Corrosion::Generator)
    message(STATUS "Using Corrosion as a subdirectory")
endif()

get_property(
    RUSTC_EXECUTABLE
    TARGET Rust::Rustc PROPERTY IMPORTED_LOCATION
)

get_property(
    CARGO_EXECUTABLE
    TARGET Rust::Cargo PROPERTY IMPORTED_LOCATION
)

# Note: Legacy function, used when respecting the `XYZ_OUTPUT_DIRECTORY` target properties is not
# possible.
function(_corrosion_set_imported_location_legacy target_name base_property filename)
    if(CMAKE_CONFIGURATION_TYPES AND CMAKE_VERSION VERSION_GREATER_EQUAL 3.20.0)
        foreach(config_type ${CMAKE_CONFIGURATION_TYPES})
            set(binary_root "${CMAKE_CURRENT_BINARY_DIR}/${config_type}")
            string(TOUPPER "${config_type}" config_type_upper)
            message(DEBUG "Setting ${base_property}_${config_type_upper} for target ${target_name}"
                    " to `${binary_root}/${filename}`.")
            # For Multiconfig we want to specify the correct location for each configuration
            set_property(
                TARGET ${target_name}
                PROPERTY "${base_property}_${config_type_upper}"
                    "${binary_root}/${filename}"
            )
        endforeach()
    else()
        set(binary_root "${CMAKE_CURRENT_BINARY_DIR}")
    endif()

    message(DEBUG "Setting ${base_property} for target ${target_name}"
                " to `${binary_root}/${filename}`.")

    # IMPORTED_LOCATION must be set regardless of possible overrides. In the multiconfig case,
    # the last configuration "wins".
    set_property(
            TARGET ${target_name}
            PROPERTY "${base_property}" "${binary_root}/${filename}"
        )
endfunction()

# Do not call this function directly!
#
# This function should be called deferred to evaluate target properties late in the configure stage.
# IMPORTED_LOCATION does not support Generator expressions, so we must evaluate the output
# directory target property value at configure time. This function must be deferred to the end of
# the configure stage, so we can be sure that the output directory is not modified afterwards.
function(_corrosion_set_imported_location_deferred target_name base_property output_directory_property filename)
    # The output directory property is expected to be set on the exposed target (without postfix),
    # but we need to set the imported location on the actual library target with postfix.
    if("${target_name}" MATCHES "^([^-]+)-(static|shared)$")
        set(output_dir_prop_target_name "${CMAKE_MATCH_1}")
    else()
        set(output_dir_prop_target_name "${target_name}")
    endif()

    get_target_property(output_directory "${output_dir_prop_target_name}" "${output_directory_property}")
    message(DEBUG "Output directory property (target ${output_dir_prop_target_name}): ${output_directory_property} dir: ${output_directory}")

    if(CMAKE_CONFIGURATION_TYPES AND CMAKE_VERSION VERSION_GREATER_EQUAL 3.20.0)
        foreach(config_type ${CMAKE_CONFIGURATION_TYPES})
            string(TOUPPER "${config_type}" config_type_upper)
            get_target_property(output_dir_curr_config "${output_dir_prop_target_name}"
                "${output_directory_property}_${config_type_upper}"
            )
            if(output_dir_curr_config)
                set(curr_out_dir "${output_dir_curr_config}")
            elseif(output_directory)
                set(curr_out_dir "${output_directory}")
            else()
                set(curr_out_dir "${CMAKE_CURRENT_BINARY_DIR}/${config_type}")
            endif()
            message(DEBUG "Setting ${base_property}_${config_type_upper} for target ${target_name}"
                    " to `${curr_out_dir}/${filename}`.")
            # For Multiconfig we want to specify the correct location for each configuration
            set_property(
                TARGET ${target_name}
                PROPERTY "${base_property}_${config_type_upper}"
                    "${curr_out_dir}/${filename}"
            )
            set(base_output_directory "${curr_out_dir}")
        endforeach()
    elseif(CMAKE_CONFIGURATION_TYPES)
        # Fallback path needed for MSVC + CMake < 3.20
        if(output_directory)
            string(GENEX_STRIP "${output_directory}" stripped_output_dir)
            if("${stripped_output_dir}" STREQUAL "${output_directory}")
                # Output directory does not contain a genex and can be respected.
                set(base_output_directory "${output_directory}")
            else()
                # Fallback to default directory if output_dir contains a genex.
                set(base_output_directory "${CMAKE_CURRENT_BINARY_DIR}")
            endif()
        else()
            # Fallback to default directory.
            set(base_output_directory "${CMAKE_CURRENT_BINARY_DIR}")
        endif()
    else()
        if(output_directory)
            set(base_output_directory "${output_directory}")
        else()
            set(base_output_directory "${CMAKE_CURRENT_BINARY_DIR}")
        endif()
    endif()

    message(DEBUG "Setting ${base_property} for target ${target_name}"
                " to `${base_output_directory}/${filename}`.")

    # IMPORTED_LOCATION must be set regardless of possible overrides. In the multiconfig case,
    # the last configuration "wins" (IMPORTED_LOCATION is not documented to have Genex support).
    set_property(
            TARGET ${target_name}
            PROPERTY "${base_property}" "${base_output_directory}/${filename}"
        )
endfunction()

# Helper function to call _corrosion_set_imported_location_deferred while eagerly
# evaluating arguments.
# Refer to https://cmake.org/cmake/help/latest/command/cmake_language.html#deferred-call-examples
function(_corrosion_call_set_imported_location_deferred target_name base_property output_directory_property filename)
    cmake_language(EVAL CODE "
        cmake_language(DEFER
            CALL
            _corrosion_set_imported_location_deferred
            [[${target_name}]]
            [[${base_property}]]
            [[${output_directory_property}]]
            [[${filename}]]
        )
    ")
endfunction()

# Set the imported location of a Rust target.
#
# Rust targets are built via custom targets / custom commands. The actual artifacts are exposed
# to CMake as imported libraries / executables that depend on the cargo_build command. For CMake
# to find the built artifact we need to set the IMPORTED location to the actual location on disk.
# Corrosion tries to copy the artifacts built by cargo to standard locations. The IMPORTED_LOCATION
# is set to point to the copy, and not the original from the cargo build directory.
#
# Parameters:
# - target_name: Name of the Rust target
# - base_property: Name of the base property - i.e. `IMPORTED_LOCATION` or `IMPORTED_IMPLIB`.
# - output_directory_property: Target property name that determines the standard location for the
#    artifact.
# - filename of the artifact.
function(_corrosion_set_imported_location target_name base_property output_directory_property filename)
    if(CORROSION_RESPECT_OUTPUT_DIRECTORY)
        _corrosion_call_set_imported_location_deferred("${target_name}" "${base_property}" "${output_directory_property}" "${filename}")
    else()
        _corrosion_set_imported_location_legacy("${target_name}" "${base_property}" "${filename}")
    endif()
endfunction()

function(_corrosion_copy_byproduct_legacy target_name cargo_build_dir file_names)
    if(ARGN)
        message(FATAL_ERROR "Unexpected additional arguments")
    endif()

    if(CMAKE_CONFIGURATION_TYPES AND CMAKE_VERSION VERSION_GREATER_EQUAL 3.20.0)
        set(output_dir "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>")
    else()
        set(output_dir "${CMAKE_CURRENT_BINARY_DIR}")
    endif()

    list(TRANSFORM file_names PREPEND "${cargo_build_dir}/" OUTPUT_VARIABLE src_file_names)
    list(TRANSFORM file_names PREPEND "${output_dir}/" OUTPUT_VARIABLE dst_file_names)
    message(DEBUG "Adding command to copy byproducts `${file_names}` to ${dst_file_names}")
    add_custom_command(TARGET _cargo-build_${target_name}
                        POST_BUILD
                        COMMAND  ${CMAKE_COMMAND} -E make_directory "${output_dir}"
                        COMMAND
                        ${CMAKE_COMMAND} -E copy_if_different
                            # tested to work with both multiple files and paths with spaces
                            ${src_file_names}
                            "${output_dir}"
                        BYPRODUCTS ${dst_file_names}
                        COMMENT "Copying byproducts `${file_names}` to ${output_dir}"
                        VERBATIM
                        COMMAND_EXPAND_LISTS
    )
endfunction()

function(_corrosion_copy_byproduct_deferred target_name output_dir_prop_name cargo_build_dir file_names)
    if(ARGN)
        message(FATAL_ERROR "Unexpected additional arguments")
    endif()
    get_target_property(output_dir ${target_name} "${output_dir_prop_name}")

    # A Genex expanding to the output directory depending on the configuration.
    set(multiconfig_out_dir_genex "")

    foreach(config_type ${CMAKE_CONFIGURATION_TYPES})
        string(TOUPPER "${config_type}" config_type_upper)
        get_target_property(output_dir_curr_config ${target_name} "${output_dir_prop_name}_${config_type_upper}")

        if(output_dir_curr_config)
            set(curr_out_dir "${output_dir_curr_config}")
        elseif(output_dir)
            # Fallback to `output_dir` if specified
            # Note: Multi-configuration generators append a per-configuration subdirectory to the
            # specified directory unless a generator expression is used (from CMake documentation).
            set(curr_out_dir "${output_dir}")
        else()
            # Fallback to default directory.
            set(curr_out_dir "${CMAKE_CURRENT_BINARY_DIR}/${config_type}")
        endif()
        set(multiconfig_out_dir_genex "${multiconfig_out_dir_genex}$<$<CONFIG:${config_type}>:${curr_out_dir}>")
    endforeach()

    if(CMAKE_CONFIGURATION_TYPES AND CMAKE_VERSION VERSION_GREATER_EQUAL 3.20.0)
        set(output_dir "${multiconfig_out_dir_genex}")
    elseif(CMAKE_CONFIGURATION_TYPES)
        # Fallback support for MSVC with CMake < 3.20.0 (byproducts may not contain genexes)
        if(output_dir)
            string(GENEX_STRIP "${output_dir}" stripped_output_dir)
            if(NOT ("${stripped_output_dir}" STREQUAL "${output_dir}"))
                 # Fallback to default directory if output_dir contains a genex.
                set(output_dir "${CMAKE_CURRENT_BINARY_DIR}")
            endif()
        else()
            # Fallback to default directory.
            set(output_dir "${CMAKE_CURRENT_BINARY_DIR}")
        endif()
    else()
        if(NOT output_dir)
            # Fallback to default directory.
            set(output_dir "${CMAKE_CURRENT_BINARY_DIR}")
        endif()
    endif()

    list(TRANSFORM file_names PREPEND "${cargo_build_dir}/" OUTPUT_VARIABLE src_file_names)
    list(TRANSFORM file_names PREPEND "${output_dir}/" OUTPUT_VARIABLE dst_file_names)
    message(DEBUG "Adding command to copy byproducts `${file_names}` to ${dst_file_names}")
    add_custom_command(TARGET _cargo-build_${target_name}
                        POST_BUILD
                        # output_dir may contain a Generator expression.
                        COMMAND  ${CMAKE_COMMAND} -E make_directory "${output_dir}"
                        COMMAND
                        ${CMAKE_COMMAND} -E copy_if_different
                            # tested to work with both multiple files and paths with spaces
                            ${src_file_names}
                            "${output_dir}"
                        BYPRODUCTS ${dst_file_names}
                        COMMENT "Copying byproducts `${file_names}` to ${output_dir}"
                        VERBATIM
                        COMMAND_EXPAND_LISTS
    )
endfunction()

function(_corrosion_call_copy_byproduct_deferred target_name output_dir_prop_name cargo_build_dir file_names)
    cmake_language(EVAL CODE "
        cmake_language(DEFER
            CALL
            _corrosion_copy_byproduct_deferred
            [[${target_name}]]
            [[${output_dir_prop_name}]]
            [[${cargo_build_dir}]]
            [[${file_names}]]
        )
    ")
endfunction()

# Copy the artifacts generated by cargo to the appropriate destination.
#
# Parameters:
# - target_name: The name of the Rust target
# - output_dir_prop_name: The property name controlling the destination (e.g.
#   `RUNTIME_OUTPUT_DIRECTORY`)
# - cargo_build_dir: the directory cargo build places it's output artifacts in.
# - filenames: the file names of any output artifacts as a list.
function(_corrosion_copy_byproducts target_name output_dir_prop_name cargo_build_dir filenames)
    if(CORROSION_RESPECT_OUTPUT_DIRECTORY)
        _corrosion_call_copy_byproduct_deferred("${target_name}" "${output_dir_prop_name}" "${cargo_build_dir}" "${filenames}")
    else()
        _corrosion_copy_byproduct_legacy("${target_name}" "${cargo_build_dir}" "${filenames}")
    endif()
endfunction()

# The Rust target triple and C target may mismatch (slightly) in some rare usecases.
# So instead of relying on CMake to provide System information, we parse the Rust target triple,
# since that is relevant for determining which libraries the Rust code requires for linking.
function(_corrosion_parse_platform manifest rust_version target_triple)

    # If the target_triple is a path to a custom target specification file, then strip everything
    # except the filename from `target_triple`.
    get_filename_component(target_triple_ext "${target_triple}" EXT)
    if(target_triple_ext)
        if(target_triple_ext STREQUAL ".json")
            get_filename_component(target_triple "${target_triple}"  NAME_WE)
        endif()
    endif()

    # The vendor part may be left out from the target triple, and since `env` is also optional,
    # we determine if vendor is present by matching against a list of known vendors.
    set(known_vendors "apple"
        "esp" # riscv32imc-esp-espidf
        "fortanix"
        "kmc"
        "pc"
        "nintendo"
        "nvidia"
        "openwrt"
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
                        "Corrosion attempts to link required C libraries depending on the OS "
                        "specified in the Rust target-triple for Linux, MacOS and windows.\n"
                        "Note: If you are targeting a different OS you can surpress this warning by"
                        " setting the CMake cache variable "
                        "`CORROSION_NO_WARN_PARSE_TARGET_TRIPLE_FAILED`."
                        "Please consider opening an issue on github if you encounter this warning."
        )
    endif()

    set(target_arch "${CMAKE_MATCH_1}")
    set(target_vendor "${CMAKE_MATCH_3}")
    set(os "${CMAKE_MATCH_4}")
    set(env "${CMAKE_MATCH_6}")

    message(DEBUG "Parsed Target triple: arch: ${target_arch}, vendor: ${target_vendor}, "
            "OS: ${os}, env: ${env}")

    set(libs "")

    set(is_windows FALSE)
    set(is_windows_msvc FALSE)
    set(is_windows_gnu FALSE)
    set(is_macos FALSE)

    if(os STREQUAL "windows")
        set(is_windows TRUE)

        if(NOT COR_NO_STD)
          list(APPEND libs "advapi32" "userenv" "ws2_32")
        endif()

        if(env STREQUAL "msvc")
            set(is_windows_msvc TRUE)

            if(NOT COR_NO_STD)
              list(APPEND libs "$<$<CONFIG:Debug>:msvcrtd>")
              # CONFIG takes a comma seperated list starting with CMake 3.19, but we still need to
              # support older CMake versions.
              set(config_is_release "$<OR:$<CONFIG:Release>,$<CONFIG:MinSizeRel>,$<CONFIG:RelWithDebInfo>>")
              list(APPEND libs "$<${config_is_release}:msvcrt>")
            endif()
        elseif(env STREQUAL "gnu")
            set(is_windows_gnu TRUE)

            if(NOT COR_NO_STD)
              list(APPEND libs "gcc_eh" "pthread")
            endif()
        endif()

        if(NOT COR_NO_STD)
          if(rust_version VERSION_LESS "1.33.0")
              list(APPEND libs "shell32" "kernel32")
          endif()

          if(rust_version VERSION_GREATER_EQUAL "1.57.0")
              list(APPEND libs "bcrypt")
          endif()
        endif()
    elseif(target_vendor STREQUAL "apple" AND os STREQUAL "darwin")
        set(is_macos TRUE)

        if(NOT COR_NO_STD)
           list(APPEND libs "System" "resolv" "c" "m")
        endif()
    elseif(os STREQUAL "linux")
        if(NOT COR_NO_STD)
           list(APPEND libs "dl" "rt" "pthread" "gcc_s" "c" "m" "util")
        endif()
    endif()

    set_source_files_properties(
        ${manifest}
        PROPERTIES
            CORROSION_PLATFORM_LIBS "${libs}"

            CORROSION_PLATFORM_IS_WINDOWS "${is_windows}"
            CORROSION_PLATFORM_IS_WINDOWS_MSVC "${is_windows_msvc}"
            CORROSION_PLATFORM_IS_WINDOWS_GNU "${is_windows_gnu}"
            CORROSION_PLATFORM_IS_MACOS "${is_macos}"
    )
endfunction()

# Add targets for the static and/or shared libraries of the rust target.
# The generated byproduct names are returned via the `out_lib_byproducts` variable name.
function(_corrosion_add_library_target workspace_manifest_path target_name has_staticlib has_cdylib
        out_archive_output_byproducts out_shared_lib_byproduct out_pdb_byproduct)

    if(NOT (has_staticlib OR has_cdylib))
        message(FATAL_ERROR "Unknown library type")
    endif()

    get_source_file_property(is_windows ${workspace_manifest_path} CORROSION_PLATFORM_IS_WINDOWS)
    get_source_file_property(is_windows_msvc ${workspace_manifest_path} CORROSION_PLATFORM_IS_WINDOWS_MSVC)
    get_source_file_property(is_windows_gnu ${workspace_manifest_path} CORROSION_PLATFORM_IS_WINDOWS_GNU)
    get_source_file_property(is_macos ${workspace_manifest_path} CORROSION_PLATFORM_IS_MACOS)

    # target file names
    string(REPLACE "-" "_" lib_name "${target_name}")

    if(is_windows_msvc)
        set(static_lib_name "${lib_name}.lib")
    else()
        set(static_lib_name "lib${lib_name}.a")
    endif()

    if(is_windows)
        set(dynamic_lib_name "${lib_name}.dll")
    elseif(is_macos)
        set(dynamic_lib_name "lib${lib_name}.dylib")
    else()
        set(dynamic_lib_name "lib${lib_name}.so")
    endif()

    if(is_windows_msvc)
        set(implib_name "${lib_name}.dll.lib")
    elseif(is_windows_gnu)
        set(implib_name "lib${lib_name}.dll.a")
    elseif(is_windows)
        message(FATAL_ERROR "Unknown windows environment - Can't determine implib name")
    endif()


    set(pdb_name "${lib_name}.pdb")

    set(archive_output_byproducts "")
    if(has_staticlib)
        list(APPEND archive_output_byproducts ${static_lib_name})
    endif()

    if(has_cdylib)
        set(${out_shared_lib_byproduct} "${dynamic_lib_name}" PARENT_SCOPE)
        if(is_windows)
            list(APPEND archive_output_byproducts ${implib_name})
        endif()
        if(is_windows_msvc)
            set(${out_pdb_byproduct} "${pdb_name}" PARENT_SCOPE)
        endif()
    endif()
    set(${out_archive_output_byproducts} "${archive_output_byproducts}" PARENT_SCOPE)

    if(has_staticlib)
        add_library(${target_name}-static STATIC IMPORTED GLOBAL)
        add_dependencies(${target_name}-static cargo-build_${target_name})

        _corrosion_set_imported_location("${target_name}-static" "IMPORTED_LOCATION"
                "ARCHIVE_OUTPUT_DIRECTORY"
                "${static_lib_name}")

        get_source_file_property(libs ${workspace_manifest_path} CORROSION_PLATFORM_LIBS)

        if(libs)
            set_property(
                    TARGET ${target_name}-static
                    PROPERTY INTERFACE_LINK_LIBRARIES ${libs}
            )
            if(is_macos)
                set_property(TARGET ${target_name}-static
                        PROPERTY INTERFACE_LINK_DIRECTORIES "/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/lib"
                        )
            endif()
        endif()
    endif()

    if(has_cdylib)
        add_library(${target_name}-shared SHARED IMPORTED GLOBAL)
        add_dependencies(${target_name}-shared cargo-build_${target_name})

        # Todo: (Not new issue): What about IMPORTED_SONAME and IMPORTED_NO_SYSTEM?
        _corrosion_set_imported_location("${target_name}-shared" "IMPORTED_LOCATION"
                "LIBRARY_OUTPUT_DIRECTORY"
                "${dynamic_lib_name}"
        )

        if(is_windows)
            _corrosion_set_imported_location("${target_name}-shared" "IMPORTED_IMPLIB"
                    "ARCHIVE_OUTPUT_DIRECTORY"
                    "${implib_name}"
            )
        endif()

        if(is_macos)
            set_property(TARGET ${target_name}-shared
                    PROPERTY INTERFACE_LINK_DIRECTORIES "/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/lib"
                    )
        endif()
    endif()

    add_library(${target_name} INTERFACE)

    if(has_cdylib AND has_staticlib)
        if(BUILD_SHARED_LIBS)
            target_link_libraries(${target_name} INTERFACE ${target_name}-shared)
        else()
            target_link_libraries(${target_name} INTERFACE ${target_name}-static)
        endif()
    elseif(has_cdylib)
        target_link_libraries(${target_name} INTERFACE ${target_name}-shared)
    else()
        target_link_libraries(${target_name} INTERFACE ${target_name}-static)
    endif()
endfunction()

function(_corrosion_add_bin_target workspace_manifest_path bin_name out_bin_byproduct out_pdb_byproduct)
    if(NOT bin_name)
        message(FATAL_ERROR "No bin_name in _corrosion_add_bin_target for target ${target_name}")
    endif()

    string(REPLACE "-" "_" bin_name_underscore "${bin_name}")

    set(pdb_name "${bin_name_underscore}.pdb")

    get_source_file_property(is_windows ${workspace_manifest_path} CORROSION_PLATFORM_IS_WINDOWS)
    get_source_file_property(is_windows_msvc ${workspace_manifest_path} CORROSION_PLATFORM_IS_WINDOWS_MSVC)
    get_source_file_property(is_macos ${workspace_manifest_path} CORROSION_PLATFORM_IS_MACOS)

    if(is_windows_msvc)
        set(${out_pdb_byproduct} "${pdb_name}" PARENT_SCOPE)
    endif()

    if(is_windows)
        set(bin_filename "${bin_name}.exe")
    else()
        set(bin_filename "${bin_name}")
    endif()
    set(${out_bin_byproduct} "${bin_filename}" PARENT_SCOPE)


    # Todo: This is compatible with the way corrosion previously exposed the bin name,
    # but maybe we want to prefix the exposed name with the package name?
    add_executable(${bin_name} IMPORTED GLOBAL)
    add_dependencies(${bin_name} cargo-build_${bin_name})

    if(is_macos)
        set_property(TARGET ${bin_name}
                PROPERTY INTERFACE_LINK_DIRECTORIES "/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/lib"
                )
    endif()

    _corrosion_set_imported_location("${bin_name}" "IMPORTED_LOCATION"
                        "RUNTIME_OUTPUT_DIRECTORY"
                        "${bin_filename}"
    )

endfunction()


if (NOT CORROSION_NATIVE_TOOLING)
    include(CorrosionGenerator)
endif()

if (CORROSION_VERBOSE_OUTPUT)
    set(_CORROSION_VERBOSE_OUTPUT_FLAG --verbose)
endif()

if(CORROSION_NATIVE_TOOLING)
    if (NOT TARGET Corrosion::Generator )
        set(_CORROSION_GENERATOR_EXE
            ${CARGO_EXECUTABLE} run --quiet --manifest-path "${CMAKE_CURRENT_LIST_DIR}/../generator/Cargo.toml" --)
        if (CORROSION_DEV_MODE)
            # If you're developing Corrosion, you want to make sure to re-configure whenever the
            # generator changes.
            file(GLOB_RECURSE _RUST_FILES CONFIGURE_DEPENDS generator/src/*.rs)
            file(GLOB _CARGO_FILES CONFIGURE_DEPENDS generator/Cargo.*)
            set_property(
                DIRECTORY APPEND
                PROPERTY CMAKE_CONFIGURE_DEPENDS
                    ${_RUST_FILES} ${_CARGO_FILES})
        endif()
    else()
        get_property(
            _CORROSION_GENERATOR_EXE
            TARGET Corrosion::Generator PROPERTY IMPORTED_LOCATION
        )
    endif()

    set(
        _CORROSION_GENERATOR
        ${CMAKE_COMMAND} -E env
            CARGO_BUILD_RUSTC=${RUSTC_EXECUTABLE}
            ${_CORROSION_GENERATOR_EXE}
                --cargo ${CARGO_EXECUTABLE}
                ${_CORROSION_VERBOSE_OUTPUT_FLAG}
        CACHE INTERNAL "corrosion-generator runner"
    )
endif()

set(_CORROSION_CARGO_VERSION ${Rust_CARGO_VERSION} CACHE INTERNAL "cargo version used by corrosion")
set(_CORROSION_RUST_CARGO_TARGET ${Rust_CARGO_TARGET} CACHE INTERNAL "target triple used by corrosion")
set(_CORROSION_RUST_CARGO_HOST_TARGET ${Rust_CARGO_HOST_TARGET} CACHE INTERNAL "host triple used by corrosion")
set(_CORROSION_RUSTC "${RUSTC_EXECUTABLE}" CACHE INTERNAL  "Path to rustc used by corrosion")
set(_CORROSION_CARGO "${CARGO_EXECUTABLE}" CACHE INTERNAL "Path to cargo used by corrosion")

string(REPLACE "-" "_" _CORROSION_RUST_CARGO_TARGET_UNDERSCORE "${Rust_CARGO_TARGET}")
string(TOUPPER "${_CORROSION_RUST_CARGO_TARGET_UNDERSCORE}" _CORROSION_TARGET_TRIPLE_UPPER)
set(_CORROSION_RUST_CARGO_TARGET_UNDERSCORE ${Rust_CARGO_TARGET} CACHE INTERNAL "lowercase target triple with underscores")
set(_CORROSION_RUST_CARGO_TARGET_UPPER
        "${_CORROSION_TARGET_TRIPLE_UPPER}"
        CACHE INTERNAL
        "target triple in uppercase with underscore"
)

# We previously specified some Custom properties as part of our public API, however the chosen names prevented us from
# supporting CMake versions before 3.19. In order to both support older CMake versions and not break existing code
# immediately, we are using a different property name depending on the CMake version. However users avoid using
# any of the properties directly, as they are no longer part of the public API and are to be considered deprecated.
# Instead use the corrosion_set_... functions as documented in the Readme.
if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.19.0)
    set(_CORR_PROP_FEATURES CORROSION_FEATURES CACHE INTERNAL "")
    set(_CORR_PROP_ALL_FEATURES CORROSION_ALL_FEATURES CACHE INTERNAL "")
    set(_CORR_PROP_NO_DEFAULT_FEATURES CORROSION_NO_DEFAULT_FEATURES CACHE INTERNAL "")
    set(_CORR_PROP_ENV_VARS CORROSION_ENVIRONMENT_VARIABLES CACHE INTERNAL "")
    set(_CORR_PROP_HOST_BUILD CORROSION_USE_HOST_BUILD CACHE INTERNAL "")
else()
    set(_CORR_PROP_FEATURES INTERFACE_CORROSION_FEATURES CACHE INTERNAL "")
    set(_CORR_PROP_ALL_FEATURES INTERFACE_CORROSION_ALL_FEATURES CACHE INTERNAL "")
    set(_CORR_PROP_NO_DEFAULT_FEATURES INTERFACE_NO_DEFAULT_FEATURES CACHE INTERNAL "")
    set(_CORR_PROP_ENV_VARS INTERFACE_CORROSION_ENVIRONMENT_VARIABLES CACHE INTERNAL "")
    set(_CORR_PROP_HOST_BUILD INTERFACE_CORROSION_USE_HOST_BUILD CACHE INTERNAL "")
endif()

# Add custom command to build one target in a package (crate)
#
# A target may be either a specific bin
function(_add_cargo_build out_cargo_build_out_dir)
    set(options "")
    set(one_value_args PACKAGE TARGET MANIFEST_PATH PROFILE TARGET_KIND)
    set(multi_value_args BYPRODUCTS)
    cmake_parse_arguments(
        ACB
        "${options}"
        "${one_value_args}"
        "${multi_value_args}"
        ${ARGN}
    )

    set(package_name "${ACB_PACKAGE}")
    set(target_name "${ACB_TARGET}")
    set(path_to_toml "${ACB_MANIFEST_PATH}")
    set(cargo_profile_name "${ACB_PROFILE}")
    set(target_kind "${ACB_TARGET_KIND}")

    if(NOT target_kind)
        message(FATAL_ERROR "TARGET_KIND not specified")
    elseif(target_kind STREQUAL "lib")
        set(cargo_rustc_filter "--lib")
    elseif(target_kind STREQUAL "bin")
        set(cargo_rustc_filter "--bin=${target_name}")
    else()
        message(FATAL_ERROR "TARGET_KIND must be `lib` or `bin`, but was `${target_kind}`")
    endif()

    if (NOT IS_ABSOLUTE "${path_to_toml}")
        set(path_to_toml "${CMAKE_SOURCE_DIR}/${path_to_toml}")
    endif()
    get_filename_component(workspace_toml_dir ${path_to_toml} DIRECTORY )

    if (CMAKE_VS_PLATFORM_NAME)
        set (build_dir "${CMAKE_VS_PLATFORM_NAME}/$<CONFIG>")
    elseif(CMAKE_CONFIGURATION_TYPES)
        set (build_dir "$<CONFIG>")
    else()
        set (build_dir .)
    endif()

    # For MSVC targets, don't mess with linker preferences.
    # TODO: We still should probably make sure that rustc is using the correct cl.exe to link programs.
    if (NOT MSVC)
        set(languages C CXX Fortran)

        set(has_compiler OFF)
        foreach(language ${languages})
            if (CMAKE_${language}_COMPILER)
                set(has_compiler ON)
            endif()
        endforeach()

        # When cross-compiling a Rust crate, at the very least we need a C linker
        if (NOT has_compiler AND CMAKE_CROSSCOMPILING)
            message(STATUS "Enabling the C compiler for linking Rust programs")
            enable_language(C)
        endif()

        # Determine the linker CMake prefers based on the enabled languages.
        set(_CORROSION_LINKER_PREFERENCE_SCORE "0")
        foreach(language ${languages})
            if( ${CMAKE_${language}_LINKER_PREFERENCE} )
                if(NOT CORROSION_LINKER_PREFERENCE
                    OR (${CMAKE_${language}_LINKER_PREFERENCE} GREATER ${_CORROSION_LINKER_PREFERENCE_SCORE}))
                    set(CORROSION_LINKER_PREFERENCE "${CMAKE_${language}_COMPILER}")
                    set(CORROSION_LINKER_PREFERENCE_TARGET "${CMAKE_${language}_COMPILER_TARGET}")
                    set(CORROSION_LINKER_PREFERENCE_LANGUAGE "${language}")
                    set(_CORROSION_LINKER_PREFERENCE_SCORE "${CMAKE_${language}_LINKER_PREFERENCE}")
                endif()
            endif()
        endforeach()
        message(VERBOSE "CORROSION_LINKER_PREFERENCE for target ${target_name}: ${CORROSION_LINKER_PREFERENCE}")
    endif()

    set(CORROSION_LINKER_PREFERENCE "c++")

    if (NOT CMAKE_CONFIGURATION_TYPES)
        set(target_dir ${CMAKE_CURRENT_BINARY_DIR})
        if (CMAKE_BUILD_TYPE STREQUAL "" OR CMAKE_BUILD_TYPE STREQUAL Debug)
            set(build_type_dir debug)
        else()
            set(build_type_dir release)
        endif()
    else()
        set(target_dir ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>)
        set(build_type_dir $<IF:$<OR:$<CONFIG:Debug>,$<CONFIG:>>,debug,release>)
    endif()

    # If a CMake sysroot is specified, forward it to the linker rustc invokes, too. CMAKE_SYSROOT is documented
    # to be passed via --sysroot, so we assume that when it's set, the linker supports this option in that style.
    if(CMAKE_CROSSCOMPILING AND CMAKE_SYSROOT)
        set(corrosion_link_args "--sysroot=${CMAKE_SYSROOT}")
    endif()

    if(COR_ALL_FEATURES)
        set(all_features_arg --all-features)
    endif()
    if(COR_NO_DEFAULT_FEATURES)
        set(no_default_features_arg --no-default-features)
    endif()
    if(COR_NO_STD)
        set(no_default_libraries_arg --no-default-libraries)
    endif()

    set(global_rustflags_target_property "$<TARGET_GENEX_EVAL:${target_name},$<TARGET_PROPERTY:${target_name},INTERFACE_CORROSION_RUSTFLAGS>>")
    set(local_rustflags_target_property  "$<TARGET_GENEX_EVAL:${target_name},$<TARGET_PROPERTY:${target_name},INTERFACE_CORROSION_LOCAL_RUSTFLAGS>>")

    set(features_target_property "$<GENEX_EVAL:$<TARGET_PROPERTY:${target_name},${_CORR_PROP_FEATURES}>>")
    set(features_genex "$<$<BOOL:${features_target_property}>:--features=$<JOIN:${features_target_property},$<COMMA>>>")

    # target property overrides corrosion_import_crate argument
    set(all_features_target_property "$<GENEX_EVAL:$<TARGET_PROPERTY:${target_name},${_CORR_PROP_ALL_FEATURES}>>")
    set(all_features_property_exists_condition "$<NOT:$<STREQUAL:${all_features_target_property},>>")
    set(all_features_property_arg "$<IF:$<BOOL:${all_features_target_property}>,--all-features,>")
    set(all_features_arg "$<IF:${all_features_property_exists_condition},${all_features_property_arg},${all_features_arg}>")

    set(no_default_features_target_property "$<GENEX_EVAL:$<TARGET_PROPERTY:${target_name},${_CORR_PROP_NO_DEFAULT_FEATURES}>>")
    set(no_default_features_property_exists_condition "$<NOT:$<STREQUAL:${no_default_features_target_property},>>")
    set(no_default_features_property_arg "$<IF:$<BOOL:${no_default_features_target_property}>,--no-default-features,>")
    set(no_default_features_arg "$<IF:${no_default_features_property_exists_condition},${no_default_features_property_arg},${no_default_features_arg}>")

    set(build_env_variable_genex "$<GENEX_EVAL:$<TARGET_PROPERTY:${target_name},${_CORR_PROP_ENV_VARS}>>")
    set(if_not_host_build_condition "$<NOT:$<BOOL:$<TARGET_PROPERTY:${target_name},${_CORR_PROP_HOST_BUILD}>>>")

    set(corrosion_link_args "$<${if_not_host_build_condition}:${corrosion_link_args}>")
    set(cargo_target_option "$<IF:${if_not_host_build_condition},--target=${_CORROSION_RUST_CARGO_TARGET},--target=${_CORROSION_RUST_CARGO_HOST_TARGET}>")
    set(target_artifact_dir "$<IF:${if_not_host_build_condition},${_CORROSION_RUST_CARGO_TARGET},${_CORROSION_RUST_CARGO_HOST_TARGET}>")

    set(flags_genex "$<GENEX_EVAL:$<TARGET_PROPERTY:${target_name},INTERFACE_CORROSION_CARGO_FLAGS>>")

    set(explicit_linker_property "$<TARGET_PROPERTY:${target_name},INTERFACE_CORROSION_LINKER>")


    # Rust will add `-lSystem` as a flag for the linker on macOS. Adding the -L flag via RUSTFLAGS only fixes the
    # problem partially - buildscripts still break, since they won't receive the RUSTFLAGS. This seems to only be a
    # problem if we specify the linker ourselves (which we do, since this is necessary for e.g. linking C++ code).
    # We can however set `LIBRARY_PATH`, which is propagated to the build-script-build properly.
    if(NOT CMAKE_CROSSCOMPILING AND CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        # not needed anymore on macos 13 (and causes issues)
        if(${CMAKE_SYSTEM_VERSION} VERSION_LESS 22)
        set(cargo_library_path "LIBRARY_PATH=/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/lib")
        endif()
    elseif(CMAKE_CROSSCOMPILING AND CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
        if(${CMAKE_HOST_SYSTEM_VERSION} VERSION_LESS 22)
            set(cargo_library_path "$<IF:${if_not_host_build_condition},,LIBRARY_PATH=/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/lib>")
        endif()
    endif()

    if(cargo_profile_name)
        set(cargo_profile "--profile=${cargo_profile_name}")
        set(build_type_dir "${cargo_profile_name}")
    else()
        set(cargo_profile $<$<NOT:$<OR:$<CONFIG:Debug>,$<CONFIG:>>>:--release>)
    endif()

    set(cargo_target_dir "${CMAKE_BINARY_DIR}/${build_dir}/cargo/build")
    set(cargo_build_dir "${cargo_target_dir}/${target_artifact_dir}/${build_type_dir}")
    set("${out_cargo_build_out_dir}" "${cargo_build_dir}" PARENT_SCOPE)

    set(features_args)
    foreach(feature ${COR_FEATURES})
        list(APPEND features_args --features ${feature})
    endforeach()

    set(flag_args)
    foreach(flag ${COR_FLAGS})
        list(APPEND flag_args ${flag})
    endforeach()

    set(corrosion_cc_rs_flags)

    corrosion_add_target_local_rustflags("${target_name}" "$<$<BOOL:${corrosion_link_args}>:-Clink-args=${corrosion_link_args}>")

    # todo: this should probably also be guarded by if_not_host_build_condition.
    if(COR_NO_STD)
        corrosion_add_target_local_rustflags("${target_name}" "-Cdefault-linker-libraries=no")
    else()
        corrosion_add_target_local_rustflags("${target_name}" "-Cdefault-linker-libraries=yes")
    endif()

    set(global_joined_rustflags "$<JOIN:${global_rustflags_target_property}, >")
    set(global_rustflags_genex "$<$<BOOL:${global_rustflags_target_property}>:RUSTFLAGS=${global_joined_rustflags}>")
    set(local_rustflags_delimiter "$<$<BOOL:${local_rustflags_target_property}>:-->")
    set(local_rustflags_genex "$<$<BOOL:${local_rustflags_target_property}>:${local_rustflags_target_property}>")


    # Used to set a linker for a specific target-triple.
    set(cargo_target_linker_var "CARGO_TARGET_${_CORROSION_RUST_CARGO_TARGET_UPPER}_LINKER")
    if(CORROSION_LINKER_PREFERENCE)
        set(cargo_target_linker_arg "$<IF:$<BOOL:${explicit_linker_property}>,${explicit_linker_property},${CORROSION_LINKER_PREFERENCE}>")
        set(cargo_target_linker "${cargo_target_linker_var}=${cargo_target_linker_arg}")
        if(CMAKE_CROSSCOMPILING)
            # CMake does not offer a host compiler we could select when configured for cross-compiling. This
            # effectively means that by default cc will be selected for builds targeting host. The user can still
            # override this by manually adding the appropriate rustflags to select the compiler for the target!
            set(cargo_target_linker "$<${if_not_host_build_condition}:${cargo_target_linker}>")
        endif()
        # Will be only set for cross-compilers like clang, c.f. `CMAKE_<LANG>_COMPILER_TARGET`.
        if(CORROSION_LINKER_PREFERENCE_TARGET)
            set(rustflag_linker_arg "-Clink-args=--target=${CORROSION_LINKER_PREFERENCE_TARGET}")
            # Skip adding the linker argument, if the linker is explicitly set, since the
            # explicit_linker_property will not be set when this function runs.
            # Passing this rustflag is necessary for clang.
            corrosion_add_target_local_rustflags("${target_name}" "$<$<NOT:$<BOOL:${explicit_linker_property}>>:${rustflag_linker_arg}>")
        endif()
    else()
        message(DEBUG "No linker preference for target ${target_name} could be detected.")
        # Note: Adding quotes here introduces wierd errors when using MSVC. Since there are no spaces here at
        # configure time, we can omit the quotes without problems
        set(cargo_target_linker $<$<BOOL:${explicit_linker_property}>:${cargo_target_linker_var}=${explicit_linker_property}>)
    endif()

    message(DEBUG "TARGET ${target_name} produces byproducts ${byproducts}")

    add_custom_target(
        _cargo-build_${target_name}
        # Build crate
        COMMAND
            ${CMAKE_COMMAND} -E env
                "${build_env_variable_genex}"
                "${global_rustflags_genex}"
                "${cargo_target_linker}"
                "${corrosion_cc_rs_flags}"
                "${cargo_library_path}"
                "CORROSION_BUILD_DIR=${CMAKE_CURRENT_BINARY_DIR}"
                "CARGO_BUILD_RUSTC=${_CORROSION_RUSTC}"
            "${_CORROSION_CARGO}"
                rustc
                ${cargo_rustc_filter}
                ${cargo_target_option}
                ${_CORROSION_VERBOSE_OUTPUT_FLAG}
                # Global --features arguments added via corrosion_import_crate()
                ${features_args}
                ${all_features_arg}
                ${no_default_features_arg}
                # Target specific features added via corrosion_set_features().
                ${features_genex}
                --package ${package_name}
                --manifest-path "${path_to_toml}"
                --target-dir "${cargo_target_dir}"
                ${cargo_profile}
                ${flag_args}
                ${flags_genex}
                # Any arguments to cargo must be placed before this line
                ${local_rustflags_delimiter}
                ${local_rustflags_genex}

        # Note: Adding `build_byproducts` (the byproducts in the cargo target directory) here
        # causes CMake to fail during the Generate stage, because the target `target_name` was not
        # found. I don't know why this happens, so we just don't specify byproducts here and
        # only specify the actual byproducts in the `POST_BUILD` custom command that copies the
        # byproducts to the final destination.
        # BYPRODUCTS  ${build_byproducts}
        # The build is conducted in the directory of the Manifest, so that configuration files such as
        # `.cargo/config.toml` or `toolchain.toml` are applied as expected.
        WORKING_DIRECTORY "${workspace_toml_dir}"
        USES_TERMINAL
        COMMAND_EXPAND_LISTS
        VERBATIM
    )

    # User exposed custom target, that depends on the internal target.
    # Corrosion post build steps are added on the internal target, which
    # ensures that they run before any user defined post build steps on this
    # target.
    add_custom_target(
        cargo-build_${target_name}
        ALL
    )
    add_dependencies(cargo-build_${target_name} _cargo-build_${target_name})

    # Add custom target before actual build that user defined custom commands (e.g. code generators) can
    # use as a hook to do something before the build. This mainly exists to not expose the `_cargo-build` targets.
    add_custom_target(cargo-prebuild_${target_name})
    add_dependencies(_cargo-build_${target_name} cargo-prebuild_${target_name})
    if(NOT TARGET cargo-prebuild)
        add_custom_target(cargo-prebuild)
    endif()
    add_dependencies(cargo-prebuild cargo-prebuild_${target_name})

    add_custom_target(
        cargo-clean_${target_name}
        COMMAND
            $<TARGET_FILE:Rust::Cargo> clean --target ${_CORROSION_RUST_CARGO_TARGET}
            -p ${package_name} --manifest-path ${path_to_toml}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/${build_dir}
        USES_TERMINAL
    )

    if (NOT TARGET cargo-clean)
        add_custom_target(cargo-clean)
    endif()
    add_dependencies(cargo-clean cargo-clean_${target_name})
endfunction()

function(corrosion_import_crate)
    set(OPTIONS ALL_FEATURES NO_DEFAULT_FEATURES NO_STD)
    set(ONE_VALUE_KEYWORDS MANIFEST_PATH PROFILE)
    set(MULTI_VALUE_KEYWORDS CRATES FEATURES FLAGS)
    cmake_parse_arguments(COR "${OPTIONS}" "${ONE_VALUE_KEYWORDS}" "${MULTI_VALUE_KEYWORDS}" ${ARGN})

    if (NOT DEFINED COR_MANIFEST_PATH)
        message(FATAL_ERROR "MANIFEST_PATH is a required keyword to corrosion_add_crate")
    endif()

    if(COR_PROFILE)
        if(Rust_VERSION VERSION_LESS 1.57.0)
            message(FATAL_ERROR "Selecting custom profiles via `PROFILE` requires at least rust 1.57.0, but you "
                        "have ${Rust_VERSION}."
        )
        else()
            set(cargo_profile --profile=${COR_PROFILE})
        endif()
    endif()

    if (NOT IS_ABSOLUTE "${COR_MANIFEST_PATH}")
        set(COR_MANIFEST_PATH ${CMAKE_CURRENT_SOURCE_DIR}/${COR_MANIFEST_PATH})
    endif()

    _corrosion_parse_platform(${COR_MANIFEST_PATH} ${Rust_VERSION} ${_CORROSION_RUST_CARGO_TARGET})

    if (CORROSION_NATIVE_TOOLING)
        execute_process(
            COMMAND
                ${_CORROSION_GENERATOR}
                    --manifest-path ${COR_MANIFEST_PATH}
                    print-root
            OUTPUT_VARIABLE toml_dir
            RESULT_VARIABLE ret)

        if (NOT ret EQUAL "0")
            message(FATAL_ERROR "corrosion-generator failed: ${ret}")
        endif()

        string(STRIP "${toml_dir}" toml_dir)

        get_filename_component(toml_dir_name ${toml_dir} NAME)

        set(
            generated_cmake
            "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_FILES_DIRECTORY}/corrosion/${toml_dir_name}.dir/cargo-build.cmake"
        )

        if (CMAKE_VS_PLATFORM_NAME)
            set (_CORROSION_CONFIGURATION_ROOT --configuration-root ${CMAKE_VS_PLATFORM_NAME})
        endif()

        set(crates_args)
        foreach(crate ${COR_CRATES})
            list(APPEND crates_args --crates ${crate})
        endforeach()

        execute_process(
            COMMAND
                ${_CORROSION_GENERATOR}
                    --manifest-path ${COR_MANIFEST_PATH}
                    gen-cmake
                        ${_CORROSION_CONFIGURATION_ROOT}
                        ${crates_args}
                        ${cargo_profile}
                        -o ${generated_cmake}
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            RESULT_VARIABLE ret)

        if (NOT ret EQUAL "0")
            message(FATAL_ERROR "corrosion-generator failed")
        endif()

        include(${generated_cmake})
    else()
        _generator_add_cargo_targets(
            MANIFEST_PATH
                "${COR_MANIFEST_PATH}"
            CRATES
                "${COR_CRATES}"
            PROFILE
                "${COR_PROFILE}"
        )
    endif()
endfunction(corrosion_import_crate)

function(corrosion_set_linker_language target_name language)
    message(DEPRECATION "corrosion_set_linker_language is deprecated."
            "Please use corrosion_set_linker and set a specific linker.")
    set_property(
        TARGET _cargo-build_${target_name}
        PROPERTY LINKER_LANGUAGE ${language}
    )
endfunction()

function(corrosion_set_linker target_name linker)
    if(NOT linker)
        message(FATAL_ERROR "The linker passed to `corrosion_set_linker` may not be empty")
    endif()
    if(MSVC)
        message(WARNING "Explicitly setting the linker with the MSVC toolchain is currently not supported and ignored")
    endif()

    set_property(
        TARGET ${target_name}
        PROPERTY INTERFACE_CORROSION_LINKER "${linker}"
    )
endfunction()

function(corrosion_set_hostbuild target_name)
    # Configure the target to be compiled for the Host target and ignore any cross-compile configuration.
    set_property(
            TARGET ${target_name}
            PROPERTY ${_CORR_PROP_HOST_BUILD} 1
    )
endfunction()

# Add flags for rustc (RUSTFLAGS) which affect the target and all of it's Rust dependencies
#
# Additional rustflags may be passed as optional parameters after rustflag.
# Please note, that if you import multiple targets from a package or workspace, but set different
# Rustflags via this function, the Rust dependencies will have to be rebuilt when changing targets.
# Consider `corrosion_add_target_local_rustflags()` as an alternative to avoid this.
function(corrosion_add_target_rustflags target_name rustflag)
    # Additional rustflags may be passed as optional parameters after rustflag.
    set_property(
            TARGET ${target_name}
            APPEND
            PROPERTY INTERFACE_CORROSION_RUSTFLAGS ${rustflag} ${ARGN}
    )
endfunction()

# Add flags for rustc (RUSTFLAGS) which only affect the target, but none of it's (Rust) dependencies
#
# Additional rustflags may be passed as optional parameters after rustc_flag.
function(corrosion_add_target_local_rustflags target_name rustc_flag)
    # Set Rustflags via `cargo rustc` which only affect the current crate, but not dependencies.
    set_property(
            TARGET ${target_name}
            APPEND
            PROPERTY INTERFACE_CORROSION_LOCAL_RUSTFLAGS ${rustc_flag} ${ARGN}
    )
endfunction()

function(corrosion_set_env_vars target_name env_var)
    # Additional environment variables may be passed as optional parameters after env_var.
    set_property(
        TARGET ${target_name}
        APPEND
        PROPERTY ${_CORR_PROP_ENV_VARS} ${env_var} ${ARGN}
    )
endfunction()

function(corrosion_set_cargo_flags target_name)
    # corrosion_set_cargo_flags(<target_name> [<flag1> ... ])

    set_property(
            TARGET ${target_name}
            APPEND
            PROPERTY INTERFACE_CORROSION_CARGO_FLAGS ${ARGN}
    )
endfunction()

function(corrosion_set_features target_name)
    # corrosion_set_features(<target_name> [ALL_FEATURES=Bool] [NO_DEFAULT_FEATURES] [FEATURES <feature1> ... ])
    set(options NO_DEFAULT_FEATURES)
    set(one_value_args ALL_FEATURES)
    set(multi_value_args FEATURES)
    cmake_parse_arguments(
            PARSE_ARGV 1
            SET
            "${options}"
            "${one_value_args}"
            "${multi_value_args}"
    )

    if(DEFINED SET_ALL_FEATURES)
        set_property(
                TARGET ${target_name}
                PROPERTY ${_CORR_PROP_ALL_FEATURES} ${SET_ALL_FEATURES}
        )
    endif()
    if(SET_NO_DEFAULT_FEATURES)
        set_property(
                TARGET ${target_name}
                PROPERTY ${_CORR_PROP_NO_DEFAULT_FEATURES} 1
        )
    endif()
    if(SET_FEATURES)
        set_property(
                TARGET ${target_name}
                APPEND
                PROPERTY ${_CORR_PROP_FEATURES} ${SET_FEATURES}
        )
    endif()
endfunction()

function(corrosion_link_libraries target_name)
    add_dependencies(_cargo-build_${target_name} ${ARGN})
    foreach(library ${ARGN})
        set_property(
            TARGET _cargo-build_${target_name}
            APPEND
            PROPERTY CARGO_DEPS_LINKER_LANGUAGES
            $<TARGET_PROPERTY:${library},LINKER_LANGUAGE>
        )

        corrosion_add_target_local_rustflags(${target_name} "-L$<TARGET_LINKER_FILE_DIR:${library}>")
        corrosion_add_target_local_rustflags(${target_name} "-l$<TARGET_LINKER_FILE_BASE_NAME:${library}>")
    endforeach()
endfunction(corrosion_link_libraries)

function(corrosion_install)
    # Default install dirs
    include(GNUInstallDirs)

    # Parse arguments to corrosion_install
    list(GET ARGN 0 INSTALL_TYPE)
    list(REMOVE_AT ARGN 0)

    # The different install types that are supported. Some targets may have more than one of these
    # types. For example, on Windows, a shared library will have both an ARCHIVE component and a
    # RUNTIME component.
    set(INSTALL_TARGET_TYPES ARCHIVE LIBRARY RUNTIME PRIVATE_HEADER PUBLIC_HEADER)

    # Arguments to each install target type
    set(OPTIONS)
    set(ONE_VALUE_ARGS DESTINATION)
    set(MULTI_VALUE_ARGS PERMISSIONS CONFIGURATIONS)
    set(TARGET_ARGS ${OPTIONS} ${ONE_VALUE_ARGS} ${MULTI_VALUE_ARGS})

    if (INSTALL_TYPE STREQUAL "TARGETS")
        # corrosion_install(TARGETS ... [EXPORT <export-name>]
        #                   [[ARCHIVE|LIBRARY|RUNTIME|PRIVATE_HEADER|PUBLIC_HEADER]
        #                    [DESTINATION <dir>]
        #                    [PERMISSIONS permissions...]
        #                    [CONFIGURATIONS [Debug|Release|...]]
        #                   ] [...])

        # Extract targets
        set(INSTALL_TARGETS)
        list(LENGTH ARGN ARGN_LENGTH)
        set(DELIMITERS EXPORT ${INSTALL_TARGET_TYPES} ${TARGET_ARGS})
        while(ARGN_LENGTH)
            # If we hit another keyword, stop - we've found all the targets
            list(GET ARGN 0 FRONT)
            if (FRONT IN_LIST DELIMITERS)
                break()
            endif()

            list(APPEND INSTALL_TARGETS ${FRONT})
            list(REMOVE_AT ARGN 0)

            # Update ARGN_LENGTH
            list(LENGTH ARGN ARGN_LENGTH)
        endwhile()

        # Check if there are any args left before proceeding
        list(LENGTH ARGN ARGN_LENGTH)
        if (ARGN_LENGTH)
            list(GET ARGN 0 FRONT)
            if (FRONT STREQUAL "EXPORT")
                list(REMOVE_AT ARGN 0) # Pop "EXPORT"

                list(GET ARGN 0 EXPORT_NAME)
                list(REMOVE_AT ARGN 0) # Pop <export-name>
                message(FATAL_ERROR "EXPORT keyword not yet implemented!")
            endif()
        endif()

        # Loop over all arguments and get options for each install target type
        list(LENGTH ARGN ARGN_LENGTH)
        while(ARGN_LENGTH)
            # Check if we're dealing with arguments for a specific install target type, or with
            # default options for all target types.
            list(GET ARGN 0 FRONT)
            if (FRONT IN_LIST INSTALL_TARGET_TYPES)
                set(INSTALL_TARGET_TYPE ${FRONT})
                list(REMOVE_AT ARGN 0)
            else()
                set(INSTALL_TARGET_TYPE DEFAULT)
            endif()

            # Gather the arguments to this install type
            set(ARGS)
            while(ARGN_LENGTH)
                # If the next keyword is an install target type, then break - arguments have been
                # gathered.
                list(GET ARGN 0 FRONT)
                if (FRONT IN_LIST INSTALL_TARGET_TYPES)
                    break()
                endif()

                list(APPEND ARGS ${FRONT})
                list(REMOVE_AT ARGN 0)

                list(LENGTH ARGN ARGN_LENGTH)
            endwhile()

            # Parse the arguments and register the file install
            cmake_parse_arguments(
                COR "${OPTIONS}" "${ONE_VALUE_ARGS}" "${MULTI_VALUE_ARGS}" ${ARGS})

            if (COR_DESTINATION)
                set(COR_INSTALL_${INSTALL_TARGET_TYPE}_DESTINATION ${COR_DESTINATION})
            endif()

            if (COR_PERMISSIONS)
                set(COR_INSTALL_${INSTALL_TARGET_TYPE}_PERMISSIONS ${COR_PERMISSIONS})
            endif()

            if (COR_CONFIGURATIONS)
                set(COR_INSTALL_${INSTALL_TARGET_TYPE}_CONFIGURATIONS ${COR_CONFIGURATIONS})
            endif()

            # Update ARG_LENGTH
            list(LENGTH ARGN ARGN_LENGTH)
        endwhile()

        # Default permissions for all files
        set(DEFAULT_PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ)

        # Loop through each install target and register file installations
        foreach(INSTALL_TARGET ${INSTALL_TARGETS})
            # Don't both implementing target type differentiation using generator expressions since
            # TYPE cannot change after target creation
            get_property(
                TARGET_TYPE
                TARGET ${INSTALL_TARGET} PROPERTY TYPE
            )

            # Install executable files first
            if (TARGET_TYPE STREQUAL "EXECUTABLE")
                if (DEFINED COR_INSTALL_RUNTIME_DESTINATION)
                    set(DESTINATION ${COR_INSTALL_RUNTIME_DESTINATION})
                elseif (DEFINED COR_INSTALL_DEFAULT_DESTINATION)
                    set(DESTINATION ${COR_INSTALL_DEFAULT_DESTINATION})
                else()
                    set(DESTINATION ${CMAKE_INSTALL_BINDIR})
                endif()

                if (DEFINED COR_INSTALL_RUNTIME_PERMISSIONS)
                    set(PERMISSIONS ${COR_INSTALL_RUNTIME_PERMISSIONS})
                elseif (DEFINED COR_INSTALL_DEFAULT_PERMISSIONS)
                    set(PERMISSIONS ${COR_INSTALL_DEFAULT_PERMISSIONS})
                else()
                    set(
                        PERMISSIONS
                        ${DEFAULT_PERMISSIONS} OWNER_EXECUTE GROUP_EXECUTE WORLD_EXECUTE)
                endif()

                if (DEFINED COR_INSTALL_RUNTIME_CONFIGURATIONS)
                    set(CONFIGURATIONS CONFIGURATIONS ${COR_INSTALL_RUNTIME_CONFIGURATIONS})
                elseif (DEFINED COR_INSTALL_DEFAULT_CONFIGURATIONS)
                    set(CONFIGURATIONS CONFIGURATIONS ${COR_INSTALL_DEFAULT_CONFIGURATIONS})
                else()
                    set(CONFIGURATIONS)
                endif()

                install(
                    FILES $<TARGET_FILE:${INSTALL_TARGET}>
                    DESTINATION ${DESTINATION}
                    PERMISSIONS ${PERMISSIONS}
                    ${CONFIGURATIONS}
                )
            endif()
        endforeach()

    elseif(INSTALL_TYPE STREQUAL "EXPORT")
        message(FATAL_ERROR "install(EXPORT ...) not yet implemented")
    endif()
endfunction()

# Create a library target that will run cxxbridge on the given files on a crate
# that is assumed to contain a module with #[cxx::bridge]
# Call with corrosion_add_cxxbridge(<cxx_target> CRATE <crate> FILES [<filePath>, ...])
# <cxx_target> is the name of the target created that will host the cxx interface
# CRATE <crate> is the name of the specific imported target generated by corrosion_import_crate
# FILES [<filePath, ...] is the relative paths to files with #[cxx::bridge]. These are relative to <tomlPath>/src
# 
# Example: corrosion_add_cxxbridge(myCxxTarget CRATE someCrate FILES lib.rs)
function(corrosion_add_cxxbridge cxx_target)
    set(OPTIONS)
    set(ONE_VALUE_KEYWORDS CRATE)
    set(MULTI_VALUE_KEYWORDS FILES)
    cmake_parse_arguments(PARSE_ARGV 1 _arg "${OPTIONS}" "${ONE_VALUE_KEYWORDS}" "${MULTI_VALUE_KEYWORDS}")

    set(required_keywords CRATE FILES)
    foreach(keyword ${required_keywords})
        if(NOT DEFINED "_arg_${keyword}")
            message(FATAL_ERROR "Missing required parameter `${keyword}`.")
        elseif("${_arg_${keyword}}" STREQUAL "")
            message(FATAL_ERROR "Required parameter `${keyword}` may not be set to an empty string.")
        endif()
    endforeach()

    get_target_property(manifest_path "${_arg_CRATE}" INTERFACE_COR_PACKAGE_MANIFEST_PATH)

    if(NOT EXISTS "${manifest_path}")
        message(FATAL_ERROR "Internal error: No package manifest found at ${manifest_path}")
    endif()

    get_filename_component(manifest_dir ${manifest_path} DIRECTORY)

    execute_process(COMMAND cargo tree -i cxx --depth=0
        WORKING_DIRECTORY "${manifest_dir}"
        RESULT_VARIABLE cxx_version_result
        OUTPUT_VARIABLE cxx_version_output
    )
    if(NOT "${cxx_version_result}" EQUAL "0")
        message(FATAL_ERROR "Crate ${_arg_CRATE} does not depend on cxx.")
    endif()
    if(cxx_version_output MATCHES "cxx v([0-9]+.[0-9]+.[0-9]+)")
        set(cxx_required_version "${CMAKE_MATCH_1}")
    else()
        message(FATAL_ERROR "Failed to parse cxx version from cargo tree output: `cxx_version_output`")
    endif()

    # First check if a suitable version of cxxbridge is installed
    find_program(INSTALLED_CXXBRIDGE cxxbridge PATHS "$ENV{HOME}/.cargo/bin/")
    mark_as_advanced(INSTALLED_CXXBRIDGE)
    if(INSTALLED_CXXBRIDGE)
        execute_process(COMMAND ${INSTALLED_CXXBRIDGE} --version OUTPUT_VARIABLE cxxbridge_version_output)
        if(cxxbridge_version_output MATCHES "cxxbridge ([0-9]+.[0-9]+.[0-9]+)")
            set(cxxbridge_version "${CMAKE_MATCH_1}")
        else()
            set(cxxbridge_version "")
        endif()
    endif()

    set(cxxbridge "")
    if(cxxbridge_version)
        if(cxxbridge_version VERSION_EQUAL cxx_required_version)
            set(cxxbridge "${INSTALLED_CXXBRIDGE}")
        endif()
    endif()

    # No suitable version of cxxbridge was installed, so use custom target to build correct version.
    if(NOT cxxbridge)
        if(NOT TARGET "cxxbridge_v${cxx_required_version}")
            add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/corrosion/cxxbridge_v${cxx_required_version}/bin/cxxbridge"
                COMMAND
                ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/corrosion/cxxbridge_v${cxx_required_version}"
                COMMAND ${_CORROSION_CARGO} install
                    cxxbridge-cmd
                    --version "${cxx_required_version}"
                    --root "${CMAKE_BINARY_DIR}/corrosion/cxxbridge_v${cxx_required_version}"
                    --quiet
                    # todo: use --target-dir to potentially reuse artifacts
                COMMENT "Building cxxbridge (version ${cxx_required_version})"
                )
            add_custom_target("cxxbridge_v${cxx_required_version}"
                DEPENDS "${CMAKE_BINARY_DIR}/corrosion/cxxbridge_v${cxx_required_version}/bin/cxxbridge"
                )
        endif()
        add_dependencies(_cargo-build_${_arg_CRATE} "cxxbridge_v${cxx_required_version}")
        set(cxxbridge "${CMAKE_BINARY_DIR}/corrosion/cxxbridge_v${cxx_required_version}/bin/cxxbridge")
    endif()


    # The generated folder structure will be of the following form
    #
    #    CMAKE_CURRENT_BINARY_DIR
    #        corrosion_generated
    #            cxxbridge
    #                <cxx_target>
    #                    include
    #                        <cxx_target>
    #                            <headers>
    #                    src
    #                        <sourcefiles>
    #            cbindgen
    #                ...
    #            other
    #                ...

    set(corrosion_generated_dir "${CMAKE_CURRENT_BINARY_DIR}/corrosion_generated")
    set(generated_dir "${corrosion_generated_dir}/cxxbridge/${cxx_target}")
    set(header_placement_dir "${generated_dir}/include/${cxx_target}")
    set(source_placement_dir "${generated_dir}/src")
    
    add_library(${cxx_target})
    target_include_directories(${cxx_target}
        PUBLIC
            $<BUILD_INTERFACE:${generated_dir}/include>
            $<INSTALL_INTERFACE:include>
    )
    target_link_libraries(${cxx_target} PRIVATE ${_arg_CRATE})

    foreach(filepath ${_arg_FILES})
        get_filename_component(filename ${filepath} NAME_WE)
        get_filename_component(directory ${filepath} DIRECTORY)
        # todo: convert potentially absolute paths to relative paths..
        set(cxx_header ${directory}/${filename}.h)
        set(cxx_source ${directory}/${filename}.cpp)

        # todo: not all projects may use the `src` directory.
        set(rust_source_path "${manifest_dir}/src/${filepath}")

        add_custom_command(
            TARGET _cargo-build_${_arg_CRATE}
            POST_BUILD
            COMMAND
                "${CMAKE_COMMAND}" -E make_directory ${header_placement_dir}/${directory}
            COMMAND
                "${CMAKE_COMMAND}" -E make_directory ${source_placement_dir}/${directory}
            COMMAND
                ${cxxbridge} ${rust_source_path} --header --output "${header_placement_dir}/${cxx_header}"
            COMMAND
                ${cxxbridge} ${rust_source_path} --output "${source_placement_dir}/${cxx_source}"
            BYPRODUCTS
                "${header_placement_dir}/${cxx_header}"
                "${source_placement_dir}/${cxx_source}"
            DEPENDS ${cxxbridge}
            COMMENT Generating cxx bindings for crate ${_arg_CRATE}
        )

        target_sources(${cxx_target}
            PRIVATE
                "${header_placement_dir}/${cxx_header}"
                "${source_placement_dir}/${cxx_source}"
        )
    endforeach()
endfunction(corrosion_add_cxxbridge)
