set(CMAKE_OSX_DEPLOYMENT_TARGET "10.9" CACHE STRING "Minimum OS X deployment version")

# Code signing ID on Mac. A default '-' is ad-hoc codesign.
SET(MAC_CODESIGN_ID "-" CACHE STRING "Mac code-signing identity")

# Whether to inject the "get-task-allow" entitlement, which permits debugging
# on the Mac.
SET(MAC_INJECT_GET_TASK_ALLOW ON CACHE BOOL "Inject get-task-allow on Mac")

FUNCTION(CODESIGN_ON_MAC target)
  IF(APPLE)
    IF(MAC_INJECT_GET_TASK_ALLOW)
      SET(ENTITLEMENTS "--entitlements" "${CMAKE_SOURCE_DIR}/osx/fish_debug.entitlements")
    ELSE()
      SET(ENTITLEMENTS "")
    ENDIF(MAC_INJECT_GET_TASK_ALLOW)
    ADD_CUSTOM_COMMAND(
      TARGET ${target}
      POST_BUILD
      COMMAND codesign --force --deep --options runtime ${ENTITLEMENTS} --sign "${MAC_CODESIGN_ID}" $<TARGET_FILE:${target}>
      VERBATIM
    )
  ENDIF()
ENDFUNCTION(CODESIGN_ON_MAC target)
