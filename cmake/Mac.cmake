set(CMAKE_OSX_DEPLOYMENT_TARGET "10.9" CACHE STRING "Minimum OS X deployment version")

# Code signing ID on Mac.
# If this is falsey, codesigning is disabled.
# '-' is ad-hoc codesign.
set(MAC_CODESIGN_ID "" CACHE STRING "Mac code-signing identity")

# Whether to inject the "get-task-allow" entitlement, which permits debugging
# on the Mac.
set(MAC_INJECT_GET_TASK_ALLOW ON CACHE BOOL "Inject get-task-allow on Mac")

# When building a Mac build, it is common for fish to link against a
# pcre2 built for the host platform (e.g. macOS 10.15) while fish wants
# to link for macOS 10.9. This warning would be of interest for releases,
# but is just noise for daily development. Unfortunately it has no flag
# of its own, so suppress all linker warnings in debug builds.
set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} -w")

function(CODESIGN_ON_MAC target)
  if((APPLE) AND (MAC_CODESIGN_ID))
    execute_process(COMMAND sw_vers "-productVersion" OUTPUT_VARIABLE OSX_VERSION)
    if(MAC_INJECT_GET_TASK_ALLOW)
      set(ENTITLEMENTS "--entitlements" "${CMAKE_SOURCE_DIR}/osx/fish_debug.entitlements")
    else()
      set(ENTITLEMENTS "")
    endif(MAC_INJECT_GET_TASK_ALLOW)
    if(OSX_VERSION VERSION_LESS "10.13.6")
      # `-options runtime` is only available in OS X from 10.13.6 and up
      add_custom_command(
        TARGET ${target}
        POST_BUILD
        COMMAND codesign --force --deep ${ENTITLEMENTS} --sign "${MAC_CODESIGN_ID}" $<TARGET_FILE:${target}>
        VERBATIM
      )
    else()
      add_custom_command(
        TARGET ${target}
        POST_BUILD
        COMMAND codesign --force --deep --options runtime ${ENTITLEMENTS} --sign "${MAC_CODESIGN_ID}" $<TARGET_FILE:${target}>
        VERBATIM
      )
    endif()
  endif()
endfunction(CODESIGN_ON_MAC target)
