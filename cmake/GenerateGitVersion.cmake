# git_version_gen.sh creates FISH-BUILD-VERSION-FILE if it does not exist, and updates it
# only if it is out-of-date.
execute_process(
        COMMAND "${SOURCE_DIR}/build_tools/cmake_git_version_gen.sh"
        OUTPUT_VARIABLE FISH_BUILD_VERSION)

message(STATUS "FISH_BUILD_VERSION: ${FISH_BUILD_VERSION}")

# Set up fish-build-version.h
configure_file("${SOURCE_DIR}/src/fish_build_version.h.in"
               "${BINARY_DIR}/include/fish_build_version.h")
