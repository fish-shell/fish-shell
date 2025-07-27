file(GLOB language_files LIST_DIRECTORIES false RELATIVE ${CMAKE_SOURCE_DIR}/po CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/po/*.po)
foreach(lang_file ${language_files})
  string(REGEX REPLACE ".po$" "" lang ${lang_file})
  list(APPEND languages ${lang})
endforeach()

include(FeatureSummary)

option(WITH_GETTEXT "translate messages if gettext is available" ON)
if(WITH_GETTEXT)
  find_package(Gettext)
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
