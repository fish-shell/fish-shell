# Define fish_tests.
add_executable(fish_tests EXCLUDE_FROM_ALL
               src/fish_tests.cpp)
fish_link_deps_and_sign(fish_tests)

# The "test" directory.
set(TEST_DIR ${CMAKE_CURRENT_BINARY_DIR}/test)

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

if(NOT FISH_IN_TREE_BUILD)
  # We need to symlink share/functions for the tests.
  # This should be simplified.
  add_custom_target(symlink_functions
    COMMAND ${CMAKE_COMMAND} -E create_symlink
            ${CMAKE_CURRENT_SOURCE_DIR}/share/functions
            ${CMAKE_CURRENT_BINARY_DIR}/share/functions)
  add_dependencies(tests_buildroot_target symlink_functions)
else()
  add_custom_target(symlink_functions)
endif()

# Prep the environment for running the unit tests.
add_custom_target(test_prep
    # Add directories hard-coded into the tests
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${TEST_DIR}/data
    COMMAND ${CMAKE_COMMAND} -E make_directory ${TEST_DIR}/data
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${TEST_DIR}/temp
    COMMAND ${CMAKE_COMMAND} -E make_directory ${TEST_DIR}/temp

    # Add the XDG_* directories
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${TEST_DIR}/xdg_data
    COMMAND ${CMAKE_COMMAND} -E make_directory ${TEST_DIR}/xdg_data
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${TEST_DIR}/xdg_config
    COMMAND ${CMAKE_COMMAND} -E make_directory ${TEST_DIR}/xdg_config
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${TEST_DIR}/xdg_runtime
    COMMAND ${CMAKE_COMMAND} -E make_directory ${TEST_DIR}/xdg_runtime

    DEPENDS tests_buildroot_target tests_dir
    USES_TERMINAL)

# Define our individual tests.
# Each test is conceptually independent.
# However when running all tests, we want to run them serially for sanity's sake.
# So define both a normal target, and a serial variant which enforces ordering.
foreach(TESTTYPE test serial_test)
  add_custom_target(${TESTTYPE}_low_level
    COMMAND env XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/test/xdg_data
                XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/test/xdg_config
                XDG_RUNTIME_HOME=${CMAKE_CURRENT_BINARY_DIR}/test/xdg_runtime
                ./fish_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    DEPENDS fish_tests
    USES_TERMINAL)

  add_custom_target(${TESTTYPE}_fishscript
                    COMMAND
                        cd tests &&
                        env XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/test/xdg_data
                            XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/test/xdg_config
                            XDG_RUNTIME_HOME=${CMAKE_CURRENT_BINARY_DIR}/test/xdg_runtime
                        ${TEST_ROOT_DIR}/bin/fish test.fish
                    DEPENDS test_prep
                    USES_TERMINAL)

  add_custom_target(${TESTTYPE}_interactive
      COMMAND cd tests &&
                env XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/test/xdg_data
                    XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/test/xdg_config
                    XDG_RUNTIME_HOME=${CMAKE_CURRENT_BINARY_DIR}/test/xdg_runtime
                ${TEST_ROOT_DIR}/bin/fish interactive.fish
      DEPENDS test_prep
      USES_TERMINAL)
endforeach(TESTTYPE)

# Now add a dependency chain between the serial versions.
# This ensures they run in order.
add_dependencies(serial_test_fishscript serial_test_low_level)
add_dependencies(serial_test_interactive serial_test_fishscript)


add_custom_target(serial_test_high_level
                  DEPENDS serial_test_interactive serial_test_fishscript)

# Create the 'test' target.
# Set a policy so CMake stops complaining about the name 'test'.
cmake_policy(PUSH)

if(${CMAKE_VERSION} VERSION_LESS 3.11.0 AND POLICY CMP0037)
  cmake_policy(SET CMP0037 OLD)
endif()
add_custom_target(test)
cmake_policy(POP)
add_dependencies(test serial_test_high_level)

# Group test targets into a TestTargets folder
set_property(TARGET test tests_dir
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
