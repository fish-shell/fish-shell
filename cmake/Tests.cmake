# This adds ctest support to the project
enable_testing()

# By default, ctest runs tests serially
if(NOT CTEST_PARALLEL_LEVEL)
  include(ProcessorCount)
  ProcessorCount(CORES)
  set(CTEST_PARALLEL_LEVEL ${CORES})
endif()

# Even though we are using CMake's ctest for testing, we still define our own `make test` target
# rather than use its default for many reasons:
#  * CMake doesn't run tests in-proc or even add each tests as an individual node in the ninja
#    dependency tree, instead it just bundles all tests into a target called `test` that always just
#    shells out to `ctest`, so there are no build-related benefits to not doing that ourselves.
#  * CMake devs insist that it is appropriate for `make test` to never depend on `make all`, i.e.
#    running `make test` does not require any of the binaries to be built before testing.
#  * The only way to have a test depend on a binary is to add a fake test with a name like
#    "build_fish" that executes CMake recursively to build the `fish` target.
#  * It is not possible to set top-level CTest options/settings such as CTEST_PARALLEL_LEVEL from
#    within the CMake configuration file.
#  * Circling back to the point about individual tests not being actual Makefile targets, CMake does
#    not offer any way to execute a named test via the `make`/`ninja`/whatever interface; the only
#    way to manually invoke test `foo` is to to manually run `ctest` and specify a regex matching
#    `foo` as an argument, e.g. `ctest -R ^foo$`... which is really crazy.

# Set a policy so CMake stops complaining when we use the target name "test"
cmake_policy(PUSH)
if(${CMAKE_VERSION} VERSION_LESS 3.11.0 AND POLICY CMP0037)
  cmake_policy(SET CMP0037 OLD)
endif()
add_custom_target(test
  COMMAND env CTEST_PARALLEL_LEVEL=${CTEST_PARALLEL_LEVEL}
          ${CMAKE_CTEST_COMMAND} --force-new-ctest-process
          --output-on-failure
  DEPENDS fish_tests tests_buildroot_target
  USES_TERMINAL
)
cmake_policy(POP)

# Build the low-level tests code
add_executable(fish_tests EXCLUDE_FROM_ALL
               src/fish_tests.cpp)
fish_link_deps_and_sign(fish_tests)

# The "test" directory.
set(TEST_DIR ${CMAKE_CURRENT_BINARY_DIR}/test)

# HACK: CMake is a configuration tool, not a build system. However, until CMake adds a way to
# dynamically discover tests, our options are either this or resorting to sed/awk to parse the low
# level tests source file to get the list of individual tests. Or to split each test into its own
# source file.
execute_process(
    # Build fish_tests but strip all actual dependencies on fish itself...
    COMMAND sh -c "test './low_level_tests.txt' -nt '${CMAKE_SOURCE_DIR}/src/fish_tests.cpp' || \
                            ${CMAKE_CXX_COMPILER} ${CMAKE_SOURCE_DIR}/src/fish_tests.cpp \
                            -o ${CMAKE_BINARY_DIR}/fish_tests_list \
                            -I ${CMAKE_CURRENT_BINARY_DIR} \
                            -lpthread \
                            -Wl,-undefined,dynamic_lookup,--unresolved-symbols=ignore-all"
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
# ...so we can invoke it to get a list of its tests
execute_process(
  COMMAND ./fish_tests_list --list
  OUTPUT_FILE low_level_tests.txt
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

# The directory into which fish is installed.
set(TEST_INSTALL_DIR ${TEST_DIR}/buildroot)

# The directory where the tests expect to find the fish root (./bin, etc)
set(TEST_ROOT_DIR ${TEST_DIR}/root)

# Copy tests files.
file(GLOB TESTS_FILES tests/*)
add_custom_target(tests_dir DEPENDS tests)

if(NOT FISH_IN_TREE_BUILD)
  add_custom_command(TARGET tests_dir
                       COMMAND ${CMAKE_COMMAND} -E copy_directory
                       ${CMAKE_SOURCE_DIR}/tests/ ${CMAKE_BINARY_DIR}/tests/
                       COMMENT "Copying test files to binary dir"
                       VERBATIM)

    add_dependencies(fish_tests tests_dir)
endif()

# Copy littlecheck.py
configure_file(build_tools/littlecheck.py littlecheck.py COPYONLY)

# Copy pexpect_helper.py
configure_file(build_tools/pexpect_helper.py pexpect_helper.py COPYONLY)

function(add_test_target NAME)
  add_custom_target(${NAME}
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -R "^${NAME}$$"
    DEPENDS fish_tests tests_buildroot_target
    USES_TERMINAL
  )
endfunction()

# Make the directory in which to run tests.
# Also symlink fish to where the tests expect it to be.
# Lastly put fish_test_helper there too.
add_custom_target(tests_buildroot_target
                  COMMAND ${CMAKE_COMMAND} -E make_directory ${TEST_INSTALL_DIR}
                  COMMAND DESTDIR=${TEST_INSTALL_DIR} ${CMAKE_COMMAND}
                          --build ${CMAKE_CURRENT_BINARY_DIR} --target install
                  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/fish_test_helper
                          ${TEST_INSTALL_DIR}/${CMAKE_INSTALL_PREFIX}/bin
                  COMMAND ${CMAKE_COMMAND} -E create_symlink
                          ${TEST_INSTALL_DIR}/${CMAKE_INSTALL_PREFIX}
                          ${TEST_ROOT_DIR}
                  DEPENDS fish fish_test_helper)

file(STRINGS "${CMAKE_CURRENT_BINARY_DIR}/low_level_tests.txt" LOW_LEVEL_TESTS)
foreach(LTEST ${LOW_LEVEL_TESTS})
  add_test(
    NAME ${LTEST}
    COMMAND ${CMAKE_BINARY_DIR}/fish_tests ${LTEST}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  )
  add_test_target("${LTEST}")
endforeach(LTEST)

FILE(GLOB FISH_CHECKS CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/tests/checks/*.fish)
foreach(CHECK ${FISH_CHECKS})
  get_filename_component(CHECK_NAME ${CHECK} NAME)
  get_filename_component(CHECK ${CHECK} NAME_WE)
  add_test(NAME ${CHECK_NAME}
    COMMAND sh ${CMAKE_CURRENT_BINARY_DIR}/tests/test_driver.sh
               ${CMAKE_CURRENT_BINARY_DIR}/tests/test.fish ${CHECK}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/tests
  )
  add_test_target("${CHECK_NAME}")
endforeach(CHECK)
