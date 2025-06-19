add_executable(fish_test_helper tests/fish_test_helper.c)

FILE(GLOB FISH_CHECKS CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/tests/checks/*.fish)
foreach(CHECK ${FISH_CHECKS})
  get_filename_component(CHECK_NAME ${CHECK} NAME)
  add_custom_target(
    test_${CHECK_NAME}
    COMMAND ${CMAKE_SOURCE_DIR}/tests/test_driver.py ${CMAKE_CURRENT_BINARY_DIR}
                checks/${CHECK_NAME}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/tests
    DEPENDS fish fish_indent fish_key_reader fish_test_helper
    USES_TERMINAL
  )
endforeach(CHECK)

FILE(GLOB PEXPECTS CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/tests/pexpects/*.py)
foreach(PEXPECT ${PEXPECTS})
  get_filename_component(PEXPECT ${PEXPECT} NAME)
  add_custom_target(
    test_${PEXPECT}
    COMMAND ${CMAKE_SOURCE_DIR}/tests/test_driver.py ${CMAKE_CURRENT_BINARY_DIR}
                pexpects/${PEXPECT}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/tests
    DEPENDS fish fish_indent fish_key_reader fish_test_helper
    USES_TERMINAL
  )
endforeach(PEXPECT)

# Rust stuff.
set(cargo_test_flags)
if(DEFINED ASAN)
    # Rust w/ -Zsanitizer=address requires explicitly specifying the --target triple or else linker
    # errors pertaining to asan symbols will ensue.
    if(NOT DEFINED Rust_CARGO_TARGET)
        message(FATAL_ERROR "ASAN requires defining the CMake variable Rust_CARGO_TARGET to the
            intended target triple")
    endif()
endif()
if(DEFINED TSAN)
    if(NOT DEFINED Rust_CARGO_TARGET)
        message(FATAL_ERROR "TSAN requires defining the CMake variable Rust_CARGO_TARGET to the
            intended target triple")
    endif()
endif()

if(DEFINED Rust_CARGO_TARGET)
    list(APPEND cargo_test_flags "--target" ${Rust_CARGO_TARGET})
    list(APPEND cargo_test_flags "--lib")
endif()

set(max_concurrency_flag)
if(DEFINED ENV{FISH_TEST_MAX_CONCURRENCY})
    list(APPEND max_concurrency_flag "--max-concurrency" $ENV{FISH_TEST_MAX_CONCURRENCY})
endif()

# The top-level test target is "fish_run_tests".
add_custom_target(fish_run_tests
  # TODO: This should be replaced with a unified solution, possibly build_tools/check.sh.
  COMMAND ${CMAKE_SOURCE_DIR}/tests/test_driver.py ${max_concurrency_flag} ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND env ${VARS_FOR_CARGO} cargo test --no-default-features ${CARGO_FLAGS} --workspace --target-dir ${rust_target_dir} ${cargo_test_flags}
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  DEPENDS fish fish_indent fish_key_reader fish_test_helper
  USES_TERMINAL
)
