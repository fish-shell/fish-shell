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

# CMake doesn't really support dynamic test discovery where a test harness is executed to list the
# tests it contains, making fish_tests.cpp's tests opaque to CMake (whereas littlecheck tests can be
# enumerated from the filesystem). We used to compile fish_tests.cpp without linking against
# anything (-Wl,-undefined,dynamic_lookup,--unresolved-symbols=ignore-all) to get it to print its
# tests at configuration time, but that's a little too much dark CMake magic.
#
# We now identify tests by checking against a magic regex that's #define'd as a no-op C-side.
file(READ "${CMAKE_SOURCE_DIR}/src/fish_tests.cpp" FISH_TESTS_CPP)
string(REGEX MATCHALL "TEST_GROUP\\( *\"([^\"]+)\"" "LOW_LEVEL_TESTS" "${FISH_TESTS_CPP}")
string(REGEX REPLACE "TEST_GROUP\\( *\"([^\"]+)\"" "\\1" "LOW_LEVEL_TESTS" "${LOW_LEVEL_TESTS}")
list(REMOVE_DUPLICATES LOW_LEVEL_TESTS)

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

foreach(LTEST ${LOW_LEVEL_TESTS})
  add_test(
    NAME ${LTEST}
    COMMAND ${CMAKE_BINARY_DIR}/fish_tests ${LTEST}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  )
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
