SET(languages de en fr nb nn pl pt_BR sv zh_CN)

INCLUDE(FeatureSummary)

OPTION(WITH_GETTEXT "translate messages if gettext is available" ON)
IF(WITH_GETTEXT)
  FIND_PACKAGE(Intl)
  FIND_PACKAGE(Gettext)
  IF(GETTEXT_FOUND)
      SET(HAVE_GETTEXT 1)
  ENDIF()
ENDIF()
ADD_FEATURE_INFO(gettext GETTEXT_FOUND "translate messages with gettext")

# Define translations
IF(GETTEXT_FOUND)
  FOREACH(lang ${languages})
    GETTEXT_PROCESS_PO_FILES(${lang} ALL
                             INSTALL_DESTINATION ${CMAKE_INSTALL_FULL_LOCALEDIR}
                             PO_FILES po/${lang}.po)
  ENDFOREACH()
ENDIF()
