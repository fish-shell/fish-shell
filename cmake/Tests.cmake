# Define fish_tests.
ADD_EXECUTABLE(fish_tests EXCLUDE_FROM_ALL
               src/fish_tests.cpp)
FISH_LINK_DEPS_AND_SIGN(fish_tests)

# The "test" directory.
SET(TEST_DIR ${CMAKE_CURRENT_BINARY_DIR}/test)

# The directory into which fish is installed.
SET(TEST_INSTALL_DIR ${TEST_DIR}/buildroot)

# The directory where the tests expect to find the fish root (./bin, etc)
SET(TEST_ROOT_DIR ${TEST_DIR}/root)

# Copy tests files.
FILE(GLOB TESTS_FILES tests/*)
ADD_CUSTOM_TARGET(tests_dir DEPENDS tests)

IF(NOT FISH_IN_TREE_BUILD)
    ADD_CUSTOM_COMMAND(TARGET tests_dir
                       COMMAND ${CMAKE_COMMAND} -E copy_directory
                       ${CMAKE_SOURCE_DIR}/tests/ ${CMAKE_BINARY_DIR}/tests/
                       COMMENT "Copying test files to binary dir"
                       VERBATIM)

    ADD_DEPENDENCIES(fish_tests tests_dir)
ENDIF()

# Copy littlecheck.py
CONFIGURE_FILE(build_tools/littlecheck.py littlecheck.py COPYONLY)

# Make the directory in which to run tests.
# Also symlink fish to where the tests expect it to be.
# Lastly put fish_test_helper there too.
ADD_CUSTOM_TARGET(tests_buildroot_target
                  COMMAND ${CMAKE_COMMAND} -E make_directory ${TEST_INSTALL_DIR}
                  COMMAND DESTDIR=${TEST_INSTALL_DIR} ${CMAKE_COMMAND}
                          --build ${CMAKE_CURRENT_BINARY_DIR} --target install
                  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/fish_test_helper
                          ${TEST_INSTALL_DIR}/${CMAKE_INSTALL_PREFIX}/bin
                  COMMAND ${CMAKE_COMMAND} -E create_symlink
                          ${TEST_INSTALL_DIR}/${CMAKE_INSTALL_PREFIX}
                          ${TEST_ROOT_DIR}
                  DEPENDS fish fish_test_helper)

IF(NOT FISH_IN_TREE_BUILD)
  # We need to symlink share/functions for the tests.
  # This should be simplified.
  ADD_CUSTOM_TARGET(symlink_functions
    COMMAND ${CMAKE_COMMAND} -E create_symlink
            ${CMAKE_CURRENT_SOURCE_DIR}/share/functions
            ${CMAKE_CURRENT_BINARY_DIR}/share/functions)
  ADD_DEPENDENCIES(tests_buildroot_target symlink_functions)
ELSE()
  ADD_CUSTOM_TARGET(symlink_functions)
ENDIF()

# Prep the environment for running the unit tests.
ADD_CUSTOM_TARGET(test_prep
                  COMMAND ${CMAKE_COMMAND} -E remove_directory ${TEST_DIR}/data
                  COMMAND ${CMAKE_COMMAND} -E remove_directory ${TEST_DIR}/home
                  COMMAND ${CMAKE_COMMAND} -E remove_directory ${TEST_DIR}/temp
                  COMMAND ${CMAKE_COMMAND} -E make_directory
                          ${TEST_DIR}/data ${TEST_DIR}/home ${TEST_DIR}/temp
                  DEPENDS tests_buildroot_target tests_dir
                  USES_TERMINAL)

# Define our individual tests.
# Each test is conceptually independent.
# However when running all tests, we want to run them serially for sanity's sake.
# So define both a normal target, and a serial variant which enforces ordering.
FOREACH(TESTTYPE test serial_test)
  ADD_CUSTOM_TARGET(${TESTTYPE}_low_level
    COMMAND env XDG_DATA_HOME=test/data XDG_CONFIG_HOME=test/home ./fish_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    DEPENDS fish_tests
    USES_TERMINAL)

  ADD_CUSTOM_TARGET(${TESTTYPE}_fishscript
                    COMMAND cd tests && ${TEST_ROOT_DIR}/bin/fish test.fish
                    DEPENDS test_prep
                    USES_TERMINAL)

  ADD_CUSTOM_TARGET(${TESTTYPE}_interactive
      COMMAND cd tests && ${TEST_ROOT_DIR}/bin/fish interactive.fish
      DEPENDS test_prep
      USES_TERMINAL)
ENDFOREACH(TESTTYPE)

# Now add a dependency chain between the serial versions.
# This ensures they run in order.
ADD_DEPENDENCIES(serial_test_fishscript serial_test_low_level)
ADD_DEPENDENCIES(serial_test_interactive serial_test_fishscript)


ADD_CUSTOM_TARGET(serial_test_high_level
                  DEPENDS serial_test_interactive serial_test_fishscript)

# Create the 'test' target.
# Set a policy so CMake stops complaining about the name 'test'.
CMAKE_POLICY(PUSH)

IF(${CMAKE_VERSION} VERSION_LESS 3.11.0 AND POLICY CMP0037)
  CMAKE_POLICY(SET CMP0037 OLD)
ENDIF()
ADD_CUSTOM_TARGET(test)
CMAKE_POLICY(POP)
ADD_DEPENDENCIES(test serial_test_high_level)

# Group test targets into a TestTargets folder
SET_PROPERTY(TARGET test tests_dir
                    test_low_level
                    test_fishscript
                    test_interactive
                    test_fishscript test_prep
                    tests_buildroot_target
                    serial_test_high_level
                    serial_test_low_level
                    serial_test_fishscript
                    serial_test_interactive
                    symlink_functions
             PROPERTY FOLDER cmake/TestTargets)
