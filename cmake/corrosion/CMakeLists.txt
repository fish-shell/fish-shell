cmake_minimum_required(VERSION 3.15)
project(Corrosion VERSION 0.3.0 LANGUAGES NONE)

# Default behavior:
# - If the project is being used as a subdirectory, then don't build tests and
#   don't enable any languages.
# - If this is a top level project, then build tests and enable the C++ compiler
if (NOT CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    set(_CORROSION_TOP_LEVEL OFF)
else()
    set(_CORROSION_TOP_LEVEL ON)
endif()

# ==== Corrosion Configuration ====

option(
    CORROSION_DEV_MODE
    "Enables some additional features if you're developing Corrosion"
    ${_CORROSION_TOP_LEVEL}
)

option(
    CORROSION_BUILD_TESTS
    "Build Corrosion test project"
    ${_CORROSION_TOP_LEVEL}
)

set(
  CORROSION_GENERATOR_EXECUTABLE CACHE STRING
  "Use prebuilt, non-bootstrapped corrosion-generator")
mark_as_advanced(CORROSION_GENERATOR_EXECUTABLE)

if (CORROSION_GENERATOR_EXECUTABLE)
    add_executable(Corrosion::Generator IMPORTED GLOBAL)
    set_property(
        TARGET Corrosion::Generator
        PROPERTY IMPORTED_LOCATION ${CORROSION_GENERATOR_EXECUTABLE})
    set(CORROSION_INSTALL_EXECUTABLE_DEFAULT OFF)
# If corrosion is used as a subdirectory, the CMake version is recent enough and the option is not
# explicitly disabled, then we do not need to build the Rust based parser and use the CMake based
# parser instead.
elseif((NOT _CORROSION_TOP_LEVEL)
        AND CMAKE_VERSION VERSION_GREATER_EQUAL 3.19.0
        AND NOT CORROSION_NATIVE_TOOLING
        )
    set(CORROSION_INSTALL_EXECUTABLE_DEFAULT OFF)
else()
    set(CORROSION_INSTALL_EXECUTABLE_DEFAULT ON)
endif()

option(
    CORROSION_INSTALL_EXECUTABLE
    "Controls whether corrosion-generator is installed with the package"
    ${CORROSION_INSTALL_EXECUTABLE_DEFAULT}
)
mark_as_advanced(CORROSION_INSTALL_EXECUTABLE)

if (_CORROSION_TOP_LEVEL)
    # TODO: If we don't have a language enabled, corrosion_import_crate will
    # attempt to enable a C compiler. Ideally, we could delay this somehow until
    # after we've set link libraries.
    enable_language(CXX)
endif()

# This little bit self-hosts the Corrosion toolchain to build the generator
# tool.
#
# It is strongly encouraged to install Corrosion separately and use
# `find_package(Corrosion REQUIRED)` instead if that works with your workflow.
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include(Corrosion)

# Testing
if (CORROSION_BUILD_TESTS)
    include(CTest)
    add_subdirectory(test)
endif()

# If Corrosion is a subdirectory, do not enable its install code
if (NOT _CORROSION_TOP_LEVEL)
    return()
endif()

# Installation

include(GNUInstallDirs)

if(CORROSION_INSTALL_EXECUTABLE)
    # Builds the generator executable
    corrosion_import_crate(MANIFEST_PATH generator/Cargo.toml)

    set(_CORROSION_GENERATOR_DESTINATION "${CMAKE_INSTALL_FULL_LIBEXECDIR}")

    corrosion_install(
        TARGETS corrosion-generator
        DESTINATION "${_CORROSION_GENERATOR_DESTINATION}"
    )
else()
    message(DEBUG "Not installing corrosion-generator since "
        "`CORROSION_INSTALL_EXECUTABLE` is set to ${CORROSION_INSTALL_EXECUTABLE}"
    )
endif()

# Generate the Config file
include(CMakePackageConfigHelpers)

configure_package_config_file(
    cmake/CorrosionConfig.cmake.in CorrosionConfig.cmake
    INSTALL_DESTINATION
        "${CMAKE_INSTALL_FULL_LIBDIR}/cmake/Corrosion"
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/CorrosionConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY
        SameMinorVersion # TODO: Should be SameMajorVersion when 1.0 is released
    ARCH_INDEPENDENT
)

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/CorrosionConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/CorrosionConfigVersion.cmake"
    DESTINATION
        "${CMAKE_INSTALL_FULL_LIBDIR}/cmake/Corrosion"
)

# These CMake scripts are needed both for the install and as a subdirectory
install(
    FILES
        cmake/Corrosion.cmake
        cmake/CorrosionGenerator.cmake
        cmake/FindRust.cmake
    DESTINATION
        "${CMAKE_INSTALL_FULL_DATADIR}/cmake"
)
