# This adds ctest support to the project
enable_testing()

# Put in a tests folder to reduce the top level targets in IDEs.
set(CMAKE_FOLDER tests)

# We will use 125 as a reserved exit code to indicate that a test has been skipped, i.e. it did not
# pass but it should not be considered a failed test run, either.
set(SKIP_RETURN_CODE 125)

# Even though we are using CMake's ctest for testing, we still define our own `make test` target
# rather than use its default for many reasons:
#  * CMake doesn't run tests in-proc or even add each tests as an individual node in the ninja
#    dependency tree, instead it just bundles all tests into a target called `test` that always just
#    shells out to `ctest`, so there are no build-related benefits to not doing that ourselves.
#  * CMake devs insist that it is appropriate for `make test` to never depend on `make all`, i.e.
#    running `make test` does not require any of the binaries to be built before testing.
#  * The only way to have a test depend on a binary is to add a fake test with a name like
#    "build_fish" that executes CMake recursively to build the `fish` target.
#  * Circling back to the point about individual tests not being actual Makefile targets, CMake does
#    not offer any way to execute a named test via the `make`/`ninja`/whatever interface; the only
#    way to manually invoke test `foo` is to to manually run `ctest` and specify a regex matching
#    `foo` as an argument, e.g. `ctest -R ^foo$`... which is really crazy.

# The top-level test target is "fish_run_tests".
add_custom_target(fish_run_tests
  COMMAND env FISH_FORCE_COLOR=1
          FISH_SOURCE_DIR=${CMAKE_SOURCE_DIR}
          ${CMAKE_CTEST_COMMAND} --force-new-ctest-process # --verbose
          --output-on-failure --progress
  DEPENDS tests_dir funcs_dir tests_buildroot_target
  USES_TERMINAL
)

# If CMP0037 is available, also make an alias "test" target.
# Note that this policy may not be available, in which case definining such a target silently fails.
cmake_policy(PUSH)
if(POLICY CMP0037)
  cmake_policy(SET CMP0037 OLD)
  add_custom_target(test DEPENDS fish_run_tests)
endif()
cmake_policy(POP)

# The "test" directory.
set(TEST_DIR ${CMAKE_CURRENT_BINARY_DIR}/test)

# The directory into which fish is installed.
set(TEST_INSTALL_DIR ${TEST_DIR}/buildroot)

# The directory where the tests expect to find the fish root (./bin, etc)
set(TEST_ROOT_DIR ${TEST_DIR}/root)

# Copy needed directories for out-of-tree builds
if(NOT FISH_IN_TREE_BUILD)
  add_custom_target(funcs_dir)
  add_custom_command(TARGET funcs_dir
    COMMAND mkdir -p ${CMAKE_BINARY_DIR}/share
    # Don't run ln twice or it will create a new link in the link.
    COMMAND test -e ${CMAKE_BINARY_DIR}/share/functions || ln -sf
                          ${CMAKE_SOURCE_DIR}/share/functions/ ${CMAKE_BINARY_DIR}/share/functions
                       COMMENT "Symlinking fish functions to binary dir"
                       VERBATIM)

  add_custom_target(tests_dir DEPENDS tests)
  add_custom_command(TARGET tests_dir
                       COMMAND ${CMAKE_COMMAND} -E copy_directory
                       ${CMAKE_SOURCE_DIR}/tests/ ${CMAKE_BINARY_DIR}/tests/
                       COMMENT "Copying test files to binary dir"
                       VERBATIM)
endif()

# Copy littlecheck.py
configure_file(build_tools/littlecheck.py littlecheck.py COPYONLY)

# Copy pexpect_helper.py
configure_file(build_tools/pexpect_helper.py pexpect_helper.py COPYONLY)

# Suppress generating Xcode schemes for all tests, there's too many.
set(CMAKE_XCODE_GENERATE_SCHEME 0)

# CMake being CMake, you can't just add a DEPENDS argument to add_test to make it depend on any of
# your binaries actually being built before `make test` is executed (requiring `make all` first),
# and the only dependency a test can have is on another test. So we make building fish
# prerequisites to our entire top-level `test` target.
function(add_test_target NAME)
  string(REPLACE "/" "-" NAME ${NAME})
  add_custom_target("test_${NAME}" COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -R "^${NAME}$$"
    DEPENDS tests_dir funcs_dir tests_buildroot_target USES_TERMINAL )
endfunction()

add_custom_target(tests_buildroot_target
                  # Make the directory in which to run tests:
                  COMMAND ${CMAKE_COMMAND} -E make_directory ${TEST_INSTALL_DIR}
                  COMMAND env DESTDIR=${TEST_INSTALL_DIR} ${CMAKE_COMMAND}
                          --build ${CMAKE_CURRENT_BINARY_DIR} --target install
                  # Put fish_test_helper there too:
                  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/fish_test_helper
                          ${TEST_INSTALL_DIR}/${CMAKE_INSTALL_PREFIX}/bin
                  # Also symlink fish to where the tests expect it to be:
                  COMMAND ${CMAKE_COMMAND} -E create_symlink
                          ${TEST_INSTALL_DIR}/${CMAKE_INSTALL_PREFIX}
                          ${TEST_ROOT_DIR}
                  DEPENDS fish fish_test_helper)

FILE(GLOB FISH_CHECKS CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/tests/checks/*.fish)
foreach(CHECK ${FISH_CHECKS})
  get_filename_component(CHECK_NAME ${CHECK} NAME)
  get_filename_component(CHECK ${CHECK} NAME_WE)
  add_test(NAME ${CHECK_NAME}
    COMMAND sh ${CMAKE_CURRENT_BINARY_DIR}/tests/test_driver.sh
               ${CMAKE_CURRENT_BINARY_DIR}/tests/test.fish ${CHECK}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/tests
  )
  set_tests_properties(${CHECK_NAME} PROPERTIES SKIP_RETURN_CODE ${SKIP_RETURN_CODE})
  set_tests_properties(${CHECK_NAME} PROPERTIES ENVIRONMENT FISH_FORCE_COLOR=1)
  add_test_target("${CHECK_NAME}")
endforeach(CHECK)

FILE(GLOB PEXPECTS CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/tests/pexpects/*.py)
foreach(PEXPECT ${PEXPECTS})
  get_filename_component(PEXPECT ${PEXPECT} NAME)
  add_test(NAME ${PEXPECT}
    COMMAND sh ${CMAKE_CURRENT_BINARY_DIR}/tests/test_driver.sh
      ${CMAKE_CURRENT_BINARY_DIR}/tests/interactive.fish ${PEXPECT}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/tests
  )
  set_tests_properties(${PEXPECT} PROPERTIES SKIP_RETURN_CODE ${SKIP_RETURN_CODE})
  set_tests_properties(${PEXPECT} PROPERTIES ENVIRONMENT FISH_FORCE_COLOR=1)
  add_test_target("${PEXPECT}")
endforeach(PEXPECT)

set(cargo_test_flags)
# Rust stuff.
if(DEFINED ASAN)
    # Rust w/ -Zsanitizer=address requires explicitly specifying the --target triple or else linker
    # errors pertaining to asan symbols will ensue.
    if(NOT DEFINED Rust_CARGO_TARGET)
        message(FATAL_ERROR "ASAN requires defining the CMake variable Rust_CARGO_TARGET to the
            intended target triple")
    endif()
endif()

if(DEFINED Rust_CARGO_TARGET)
    list(APPEND cargo_test_flags "--target" ${Rust_CARGO_TARGET})
    list(APPEND cargo_test_flags "--lib")
endif()

add_test(
    NAME "cargo-test"
    COMMAND env ${VARS_FOR_CARGO} cargo test ${CARGO_FLAGS} --workspace --target-dir ${rust_target_dir} ${cargo_test_flags}
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties("cargo-test" PROPERTIES SKIP_RETURN_CODE ${SKIP_RETURN_CODE})
add_test_target("cargo-test")
