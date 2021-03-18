# This adds ctest support to the project
enable_testing()

# By default, ctest runs tests serially
if(NOT CTEST_PARALLEL_LEVEL)
  include(ProcessorCount)
  ProcessorCount(CORES)
  message("CTEST_PARALLEL_LEVEL ${CORES}")
  set(CTEST_PARALLEL_LEVEL ${CORES})
endif()

# Define fish_tests.
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
  COMMAND ${CMAKE_CXX_COMPILER} ${CMAKE_SOURCE_DIR}/src/fish_tests.cpp
                            -o ${CMAKE_BINARY_DIR}/fish_tests_list
                            -I ${CMAKE_CURRENT_BINARY_DIR} # for config.h
                            -lpthread
                            # Strip actual dependency on fish code
                            -Wl,-undefined,dynamic_lookup,--unresolved-symbols=ignore-all
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
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

# CMake being CMake, you can't just add a DEPENDS argument to add_test to make it depend on any of
# your binaries actually being built before `make test` is executed (requiring `make all` first),
# and the only dependency a test can have is on another test. So we make building `fish_tests` a
# test, and then depend on this in all the low-level tests :/
add_test(build_fish_tests
         "${CMAKE_COMMAND}" --build ${CMAKE_BINARY_DIR} --target fish_tests)

file(STRINGS "${CMAKE_CURRENT_BINARY_DIR}/low_level_tests.txt" LOW_LEVEL_TESTS)
foreach(LTEST ${LOW_LEVEL_TESTS})
  add_test(
    NAME ${LTEST}
    COMMAND ${CMAKE_BINARY_DIR}/fish_tests ${LTEST}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  )

  set_tests_properties(${LTEST} PROPERTIES DEPENDS build_fish_tests)
endforeach(LTEST)

add_test(tests_buildroot_target
         "${CMAKE_COMMAND}" --build ${CMAKE_BINARY_DIR} --target tests_buildroot_target)
FILE(GLOB FISH_CHECKS CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/tests/checks/*.fish)
foreach(CHECK ${FISH_CHECKS})
  get_filename_component(CHECK_NAME ${CHECK} NAME)
  get_filename_component(CHECK ${CHECK} NAME_WE)
  add_test(NAME ${CHECK_NAME}
    COMMAND sh ${CMAKE_CURRENT_BINARY_DIR}/tests/test_driver.sh
               ${CMAKE_CURRENT_BINARY_DIR}/tests/test.fish ${CHECK}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/tests
  )

  set_tests_properties(${LTEST} PROPERTIES DEPENDS tests_buildroot_target)
endforeach(CHECK)
