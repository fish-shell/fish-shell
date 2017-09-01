# Define fish_tests.
ADD_EXECUTABLE(fish_tests src/fish_tests.cpp ${FISH_SRCS})
FISH_LINK_DEPS(fish_tests)

# Define the directory where the test tree will go.
SET(TESTS_INSTALL_DIR ${CMAKE_CURRENT_BINARY_DIR}/test/root/)

# Add a rule to symlink the tests directory.
ADD_CUSTOM_COMMAND(OUTPUT tests
                   COMMAND ln -s ${CMAKE_CURRENT_SOURCE_DIR}/tests
                           ${CMAKE_CURRENT_BINARY_DIR}/tests
                   COMMENT "Linking tests directory..."
                   VERBATIM)
ADD_CUSTOM_TARGET(tests_dir DEPENDS tests)
ADD_DEPENDENCIES(fish_tests tests_dir)

# Create the 'test' target.
# Set a policy so CMake stops complaining about the name 'test'.
CMAKE_POLICY(PUSH)
IF(POLICY CMP0037)
  CMAKE_POLICY(SET CMP0037 OLD)
ENDIF()
ADD_CUSTOM_TARGET(test COMMAND fish_tests)
CMAKE_POLICY(POP)

# Make the directory in which to run tests.
ADD_CUSTOM_TARGET(tests_root_target
                  COMMAND ${CMAKE_COMMAND} -E make_directory ${TESTS_INSTALL_DIR}
                  COMMAND DESTDIR=${CMAKE_BINARY_DIR}/tests_root ${CMAKE_COMMAND}
                          --build ${CMAKE_BINARY_DIR} --target install)
ADD_DEPENDENCIES(test tests_root_target)
