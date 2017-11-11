# Distributed under the OSI-approved BSD 3-Clause License.  See full license information in
# doc_src/license.hdr or https://cmake.org/licensing for details.

#.rst:
# CheckIncludeFiles
# -----------------
#
# Provides a macro to check if a list of one or more header files can
# be included together in ``C``.
#
# .. command:: CHECK_INCLUDE_FILES
#
#   ::
#
#     CHECK_INCLUDE_FILES("<includes>" <variable> [LANGUAGE <language>])
#
#   Check if the given ``<includes>`` list may be included together
#   in a ``C`` source file and store the result in an internal cache
#   entry named ``<variable>``.  Specify the ``<includes>`` argument
#   as a :ref:`;-list <CMake Language Lists>` of header file names.
#
# If LANGUAGE is set, the specified compiler will be used to perform the
# check. Acceptable values are C and CXX.
#
# The following variables may be set before calling this macro to modify
# the way the check is run:
#
# ``CMAKE_REQUIRED_FLAGS``
#   string of compile command line flags
# ``CMAKE_REQUIRED_DEFINITIONS``
#   list of macros to define (-DFOO=bar)
# ``CMAKE_REQUIRED_INCLUDES``
#   list of include directories
# ``CMAKE_REQUIRED_QUIET``
#   execute quietly without messages
#
# See modules :module:`CheckIncludeFile` and :module:`CheckIncludeFileCXX`
# to check for a single header file in ``C`` or ``CXX`` languages.

macro(CHECK_INCLUDE_FILES INCLUDE VARIABLE)
  if(NOT DEFINED "${VARIABLE}")
    set(CMAKE_CONFIGURABLE_FILE_CONTENT "/* */\n")

    if("x${ARGN}" STREQUAL "x")
       if(CMAKE_C_COMPILER_LOADED)
         set(_lang C)
       elseif(CMAKE_CXX_COMPILER_LOADED)
         set(_lang CXX)
       else()
         message(FATAL_ERROR "CHECK_INCLUDE_FILES needs either C or CXX language enabled")
       endif()
    elseif("x${ARGN}" MATCHES "^xLANGUAGE;([a-zA-Z]+)$")
       set(_lang "${CMAKE_MATCH_1}")
    else()
       message(FATAL_ERROR "Unknown arguments:\n  ${ARGN}\n")
    endif()

    if(_lang STREQUAL "C")
      set(src ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CheckIncludeFiles/${var}.c)
    elseif(_lang STREQUAL "CXX")
      set(src ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CheckIncludeFiles/${var}.cpp)
    else()
      message(FATAL_ERROR "Unknown language:\n  ${_lang}\nSupported languages: C, CXX.\n")
    endif()

    if(CMAKE_REQUIRED_INCLUDES)
      set(CHECK_INCLUDE_FILES_INCLUDE_DIRS "-DINCLUDE_DIRECTORIES=${CMAKE_REQUIRED_INCLUDES}")
    else()
      set(CHECK_INCLUDE_FILES_INCLUDE_DIRS)
    endif()
    set(CHECK_INCLUDE_FILES_CONTENT "/* */\n")
    set(MACRO_CHECK_INCLUDE_FILES_FLAGS ${CMAKE_REQUIRED_FLAGS})
    foreach(FILE ${INCLUDE})
      string(APPEND CMAKE_CONFIGURABLE_FILE_CONTENT
        "#include <${FILE}>\n")
    endforeach()
    string(APPEND CMAKE_CONFIGURABLE_FILE_CONTENT
      "\n\nint main(void){return 0;}\n")
    configure_file("${CMAKE_ROOT}/Modules/CMakeConfigurableFile.in"
      "${src}" @ONLY)

    set(_INCLUDE ${INCLUDE}) # remove empty elements
    if("${_INCLUDE}" MATCHES "^([^;]+);.+;([^;]+)$")
      list(LENGTH _INCLUDE _INCLUDE_LEN)
      set(_description "${_INCLUDE_LEN} include files ${CMAKE_MATCH_1}, ..., ${CMAKE_MATCH_2}")
    elseif("${_INCLUDE}" MATCHES "^([^;]+);([^;]+)$")
      set(_description "include files ${CMAKE_MATCH_1}, ${CMAKE_MATCH_2}")
    else()
      set(_description "include file ${_INCLUDE}")
    endif()

    if(NOT CMAKE_REQUIRED_QUIET)
      message(STATUS "Looking for ${_description}")
    endif()
    try_compile(${VARIABLE}
      ${CMAKE_BINARY_DIR}
      ${src}
      COMPILE_DEFINITIONS ${CMAKE_REQUIRED_DEFINITIONS}
      CMAKE_FLAGS
      -DCOMPILE_DEFINITIONS:STRING=${MACRO_CHECK_INCLUDE_FILES_FLAGS}
      "${CHECK_INCLUDE_FILES_INCLUDE_DIRS}"
      OUTPUT_VARIABLE OUTPUT)
    if(${VARIABLE})
      if(NOT CMAKE_REQUIRED_QUIET)
        message(STATUS "Looking for ${_description} - found")
      endif()
      set(${VARIABLE} 1 CACHE INTERNAL "Have include ${INCLUDE}")
      file(APPEND ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeOutput.log
        "Determining if files ${INCLUDE} "
        "exist passed with the following output:\n"
        "${OUTPUT}\n\n")
    else()
      if(NOT CMAKE_REQUIRED_QUIET)
        message(STATUS "Looking for ${_description} - not found")
      endif()
      set(${VARIABLE} "" CACHE INTERNAL "Have includes ${INCLUDE}")
      file(APPEND ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeError.log
        "Determining if files ${INCLUDE} "
        "exist failed with the following output:\n"
        "${OUTPUT}\nSource:\n${CMAKE_CONFIGURABLE_FILE_CONTENT}\n")
    endif()
  endif()
endmacro()
