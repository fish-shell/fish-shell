set(languages de en fr nb nn pl pt_BR sv zh_CN)

include(FeatureSummary)

option(WITH_GETTEXT "translate messages if gettext is available" ON)
if(WITH_GETTEXT)
  find_package(Intl QUIET)
  find_package(Gettext)
  if(GETTEXT_FOUND)
      set(HAVE_GETTEXT 1)
      include_directories(${Intl_INCLUDE_DIR})
  endif()
endif()
add_feature_info(gettext GETTEXT_FOUND "translate messages with gettext")

# Define translations
if(GETTEXT_FOUND)
  foreach(lang ${languages})
    # Our translations aren't set up entirely as CMake expects, so installation is done in
    # cmake/Install.cmake instead of using INSTALL_DESTINATION
    gettext_process_po_files(${lang} ALL
                             PO_FILES po/${lang}.po)
  endforeach()
endif()

cmake_push_check_state()
set(CMAKE_REQUIRED_INCLUDES ${CMAKE_REQUIRED_INCLUDES} ${Intl_INCLUDE_DIR})
set(CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES} ${Intl_LIBRARIES})
# libintl.h can be compiled into the stdlib on some GLibC systems
if(Intl_FOUND AND Intl_LIBRARIES)
  set(LIBINTL_INCLUDE "#include <libintl.h>")
endif()
check_cxx_source_compiles("
${LIBINTL_INCLUDE}
#include <stdlib.h>
int main () {
    extern int  _nl_msg_cat_cntr;
    int tmp = _nl_msg_cat_cntr;
    exit(tmp);
}
"
    HAVE__NL_MSG_CAT_CNTR)
cmake_pop_check_state()
