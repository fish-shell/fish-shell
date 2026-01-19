include(FeatureSummary)
include(FindRust)
find_package(Rust REQUIRED)

set(FISH_RUST_BUILD_DIR "${CMAKE_BINARY_DIR}/cargo")

if(DEFINED ASAN)
    list(APPEND CARGO_FLAGS "-Z" "build-std")
    list(APPEND FISH_CARGO_FEATURES_LIST "asan")
endif()
if(DEFINED TSAN)
    list(APPEND CARGO_FLAGS "-Z" "build-std")
    list(APPEND FISH_CARGO_FEATURES_LIST "tsan")
endif()

if (Rust_CARGO_TARGET)
    set(rust_target_dir "${FISH_RUST_BUILD_DIR}/${Rust_CARGO_TARGET}")
else()
    set(rust_target_dir "${FISH_RUST_BUILD_DIR}/${Rust_CARGO_HOST_TARGET}")
endif()

set(rust_profile $<IF:$<CONFIG:Debug>,debug,$<IF:$<CONFIG:RelWithDebInfo>,release-with-debug,release>>)

if (NOT DEFINED WITH_MESSAGE_LOCALIZATION) # Don't check for legacy options if the new one is defined, to help bisecting.
    if(DEFINED WITH_GETTEXT)
        message(FATAL_ERROR "the WITH_GETTEXT option is no longer supported, use -DWITH_MESSAGE_LOCALIZATION=ON|OFF")
    endif()
endif()
option(WITH_MESSAGE_LOCALIZATION "Build with localization support. Requires `msgfmt` to work." ON)
# Enable gettext feature unless explicitly disabled.
if(NOT DEFINED WITH_MESSAGE_LOCALIZATION OR "${WITH_MESSAGE_LOCALIZATION}")
    list(APPEND FISH_CARGO_FEATURES_LIST "localize-messages")
endif()

add_feature_info(Translation WITH_MESSAGE_LOCALIZATION "message localization (requires gettext)")

list(JOIN FISH_CARGO_FEATURES_LIST , FISH_CARGO_FEATURES)
