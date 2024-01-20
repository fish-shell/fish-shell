set(languages de en fr pl pt_BR sv zh_CN)

include(FeatureSummary)

option(WITH_GETTEXT "translate messages if gettext is available" ON)
if(WITH_GETTEXT)
  if(APPLE)
    # Fix for https://github.com/fish-shell/fish-shell/issues/5244
    # via https://gitlab.kitware.com/cmake/cmake/-/issues/18921
    set(CMAKE_FIND_FRAMEWORK_OLD ${CMAKE_FIND_FRAMEWORK})
    set(CMAKE_FIND_FRAMEWORK NEVER)
  endif()
  find_package(Intl QUIET)
  find_package(Gettext)
  if(GETTEXT_FOUND)
      set(HAVE_GETTEXT 1)
      include_directories(${Intl_INCLUDE_DIR})
  endif()
  if(APPLE)
    set(CMAKE_FIND_FRAMEWORK ${CMAKE_FIND_FRAMEWORK_OLD})
    unset(CMAKE_FIND_FRAMEWORK_OLD)
  endif()
endif()
add_feature_info(gettext GETTEXT_FOUND "translate messages with gettext")

# Define translations
if(GETTEXT_FOUND)
  # Group pofile targets into their own folder, as there's a lot of them.
  set(CMAKE_FOLDER pofiles)
  foreach(lang ${languages})
    # Our translations aren't set up entirely as CMake expects, so installation is done in
    # cmake/Install.cmake instead of using INSTALL_DESTINATION
    gettext_process_po_files(${lang} ALL
                             PO_FILES po/${lang}.po)
  endforeach()
  set(CMAKE_FOLDER)
endif()

cmake_push_check_state()
set(CMAKE_REQUIRED_INCLUDES ${CMAKE_REQUIRED_INCLUDES} ${Intl_INCLUDE_DIR})
set(CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES} ${Intl_LIBRARIES})
# libintl.h can be compiled into the stdlib on some GLibC systems
if(Intl_FOUND AND Intl_LIBRARIES)
  set(LIBINTL_INCLUDE "#include <libintl.h>")
endif()
