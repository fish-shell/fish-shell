# This is Mac-only.
if (NOT APPLE)
    return()
endif (NOT APPLE)

# The source tree containing certain macOS resources.
set(OSX_DIR ${CMAKE_CURRENT_SOURCE_DIR}/osx)

# 10.9 is the minimum supported version.
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.9")

set(RESOURCE_FILES
    ${OSX_DIR}/launch_fish.scpt
    ${OSX_DIR}/fish_term_icon.icns
    ${CMAKE_CURRENT_SOURCE_DIR}/build_tools/osx_package_scripts/add-shell
    ${OSX_DIR}/install.sh
)

# Resource files must be present in the source list.
add_executable(fish_macapp EXCLUDE_FROM_ALL
    ${OSX_DIR}/osx_fish_launcher.m
    ${RESOURCE_FILES}
)

# Compute the version. Note this is done at generation time, not build time,
# so cmake must be re-run after version changes for the app to be updated. But
# generally this will be run by make_macos_pkg.sh which always re-runs cmake.
execute_process(
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/build_tools/git_version_gen.sh
    COMMAND cut -d- -f1
    OUTPUT_VARIABLE FISH_SHORT_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE)


# Note CMake appends .app, so the real output name will be fish.app.
# This target does not include the 'base' resource.
set_target_properties(fish_macapp PROPERTIES OUTPUT_NAME "fish")

find_library(FOUNDATION_LIB Foundation)
target_link_libraries(fish_macapp ${FOUNDATION_LIB})

set_target_properties(fish_macapp PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_INFO_PLIST ${OSX_DIR}/CMakeMacAppInfo.plist.in
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.ridiculousfish.fish-shell"
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${FISH_SHORT_VERSION}
    RESOURCE "${RESOURCE_FILES}"
)

# The fish Mac app contains a fish installation inside the package.
# Here is where it gets built.
# Copy into the fish mac app after.
set(MACAPP_FISH_BUILDROOT ${CMAKE_CURRENT_BINARY_DIR}/macapp_buildroot/base)

add_custom_command(TARGET fish_macapp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${MACAPP_FISH_BUILDROOT}
    COMMAND DESTDIR=${MACAPP_FISH_BUILDROOT} ${CMAKE_COMMAND}
            --build ${CMAKE_CURRENT_BINARY_DIR} --target install
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${MACAPP_FISH_BUILDROOT}/..
            $<TARGET_BUNDLE_CONTENT_DIR:fish_macapp>/Resources/
    VERBATIM
)

# The entitlements file.
set(MACAPP_ENTITLEMENTS "${CMAKE_SOURCE_DIR}/osx/MacApp.entitlements")

# Group our targets in a folder.
set_property(TARGET fish_macapp PROPERTY FOLDER macapp)
