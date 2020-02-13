# This is Mac-only.
if (NOT APPLE)
    RETURN()
endif (NOT APPLE)

# The source tree containing certain macOS resources.
SET(OSX_DIR ${CMAKE_CURRENT_SOURCE_DIR}/osx)

SET(RESOURCE_FILES
    ${OSX_DIR}/launch_fish.scpt
    ${OSX_DIR}/fish_term_icon.icns
    ${CMAKE_CURRENT_SOURCE_DIR}/build_tools/osx_package_scripts/add-shell
    ${OSX_DIR}/install.sh
)

# Resource files must be present in the source list.
ADD_EXECUTABLE(fish_macapp EXCLUDE_FROM_ALL
    ${OSX_DIR}/osx_fish_launcher.m
    ${RESOURCE_FILES}
)

# Compute the version. Note this is done at generation time, not build time,
# so cmake must be re-run after version changes for the app to be updated. But
# generally this will be run by make_pkg.sh which always re-runs cmake.
EXECUTE_PROCESS(
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/build_tools/git_version_gen.sh --stdout
    COMMAND cut -d- -f1
    OUTPUT_VARIABLE FISH_SHORT_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE)


# Note CMake appends .app, so the real output name will be fish.app. 
# This target does not include the 'base' resource.
SET_TARGET_PROPERTIES(fish_macapp PROPERTIES OUTPUT_NAME "fish")

FIND_LIBRARY(FOUNDATION_LIB Foundation)
TARGET_LINK_LIBRARIES(fish_macapp ${FOUNDATION_LIB})

SET_TARGET_PROPERTIES(fish_macapp PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_INFO_PLIST ${OSX_DIR}/CMakeMacAppInfo.plist.in
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.ridiculousfish.fish-shell"
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${FISH_SHORT_VERSION}
    RESOURCE "${RESOURCE_FILES}"
)

# The fish Mac app contains a fish installation inside the package.
# Here is where it gets built.
# Copy into the fish mac app after.
SET(MACAPP_FISH_BUILDROOT ${CMAKE_CURRENT_BINARY_DIR}/macapp_buildroot/base)

ADD_CUSTOM_COMMAND(TARGET fish_macapp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${MACAPP_FISH_BUILDROOT}
    COMMAND DESTDIR=${MACAPP_FISH_BUILDROOT} ${CMAKE_COMMAND}
            --build ${CMAKE_CURRENT_BINARY_DIR} --target install
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${MACAPP_FISH_BUILDROOT}/..
            $<TARGET_BUNDLE_CONTENT_DIR:fish_macapp>/Resources/
    VERBATIM
)

# The entitlements file.
SET(MACAPP_ENTITLEMENTS "${CMAKE_SOURCE_DIR}/osx/MacApp.entitlements")

# Target to sign the macapp.
# Note that a POST_BUILD step happens before resources are copied,
# and therefore would be too early.
ADD_CUSTOM_TARGET(signed_fish_macapp
    DEPENDS fish_macapp "${MACAPP_ENTITLEMENTS}"
    COMMAND codesign --force --deep
            --options runtime
            --entitlements "${MACAPP_ENTITLEMENTS}"
            --sign "${MAC_CODESIGN_ID}"
            $<TARGET_BUNDLE_DIR:fish_macapp>
    VERBATIM
)
