# Define fish_tests.
ADD_EXECUTABLE(fish_tests EXCLUDE_FROM_ALL
               src/fish_tests.cpp)
FISH_LINK_DEPS(fish_tests)

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

# Create the 'test' target.
# Set a policy so CMake stops complaining about the name 'test'.
CMAKE_POLICY(PUSH)
IF(POLICY CMP0037)
  CMAKE_POLICY(SET CMP0037 OLD)
ENDIF()
ADD_CUSTOM_TARGET(test)
CMAKE_POLICY(POP)

ADD_CUSTOM_TARGET(test_low_level
  COMMAND env XDG_DATA_HOME=test/data XDG_CONFIG_HOME=test/home ./fish_tests
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  DEPENDS fish_tests
  USES_TERMINAL)
ADD_DEPENDENCIES(test test_low_level tests_dir)

# Make the directory in which to run tests.
# Also symlink fish to where the tests expect it to be.
ADD_CUSTOM_TARGET(tests_buildroot_target
                  COMMAND ${CMAKE_COMMAND} -E make_directory ${TEST_INSTALL_DIR}
                  COMMAND DESTDIR=${TEST_INSTALL_DIR} ${CMAKE_COMMAND}
                          --build ${CMAKE_CURRENT_BINARY_DIR} --target install
                  COMMAND ${CMAKE_COMMAND} -E create_symlink
                          ${TEST_INSTALL_DIR}/${CMAKE_INSTALL_PREFIX}
                          ${TEST_ROOT_DIR}
                  DEPENDS fish)

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

#
# Prep the environment for running the unit tests.
# test-prep: show-DESTDIR show-LN_S show-FISH_VERSION
#   $v rm -rf test
#   $v $(MKDIR_P) test/data test/home test/temp
# ifdef DESTDIR
#   $v $(LN_S) $(DESTDIR) test/root
# else
#   $v $(MKDIR_P) test/root
# endif
ADD_CUSTOM_TARGET(test_prep
                  COMMAND ${CMAKE_COMMAND} -E remove_directory ${TEST_DIR}/data
                  COMMAND ${CMAKE_COMMAND} -E remove_directory ${TEST_DIR}/home
                  COMMAND ${CMAKE_COMMAND} -E remove_directory ${TEST_DIR}/temp
                  COMMAND ${CMAKE_COMMAND} -E make_directory
                          ${TEST_DIR}/data ${TEST_DIR}/home ${TEST_DIR}/temp
                  DEPENDS tests_buildroot_target
                  USES_TERMINAL)


# test_high_level_test_deps = test_fishscript test_interactive test_invocation
# test_high_level: DESTDIR = $(PWD)/test/root/
# test_high_level: prefix = .
# test_high_level: test-prep install-force test_fishscript test_interactive test_invocation
# .PHONY: test_high_level
#
# test_invocation: $(call filter_up_to,test_invocation,$(active_test_goals))
#   cd tests; ./invocation.sh
# .PHONY: test_invocation
ADD_CUSTOM_TARGET(test_invocation
                  COMMAND ./invocation.sh
                  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/tests/
                  DEPENDS test_prep test_low_level
                  USES_TERMINAL)

#
# test_fishscript: $(call filter_up_to,test_fishscript,$(active_test_goals))
#   cd tests; ../test/root/bin/fish test.fish
# .PHONY: test_fishscript

ADD_CUSTOM_TARGET(test_fishscript
                  COMMAND cd tests && ${TEST_ROOT_DIR}/bin/fish test.fish
                  DEPENDS test_prep test_invocation
                  USES_TERMINAL)
#
# test_interactive: $(call filter_up_to,test_interactive,$(active_test_goals))
#   cd tests; ../test/root/bin/fish interactive.fish
# .PHONY: test_interactive

ADD_CUSTOM_TARGET(test_interactive
    COMMAND cd tests && ${TEST_ROOT_DIR}/bin/fish interactive.fish
    DEPENDS test_prep test_invocation test_fishscript
    USES_TERMINAL)

ADD_CUSTOM_TARGET(test_high_level
                  DEPENDS test_invocation test_fishscript test_interactive)
ADD_DEPENDENCIES(test test_high_level)

# Group test targets into a TestTargets folder
SET_PROPERTY(TARGET test test_low_level test_high_level tests_dir
                    test_invocation test_fishscript test_prep
                    tests_buildroot_target
                    symlink_functions
             PROPERTY FOLDER cmake/TestTargets)
