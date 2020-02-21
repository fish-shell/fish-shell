# -DLOCALEDIR="${CMAKE_INSTALL_FULL_LOCALEDIR}"
# -DPREFIX=L"${CMAKE_INSTALL_PREFIX}"
# -DDATADIR=L"${CMAKE_INSTALL_FULL_DATADIR}"
# -DSYSCONFDIR=L"${CMAKE_INSTALL_FULL_SYSCONFDIR}"
# -DBINDIR=L"${CMAKE_INSTALL_FULL_BINDIR}"
# -DDOCDIR=L"${CMAKE_INSTALL_FULL_DOCDIR}")

SET(CMAKE_INSTALL_MESSAGE NEVER)

SET(PROGRAMS fish fish_indent fish_key_reader)

SET(prefix ${CMAKE_INSTALL_PREFIX})
SET(bindir ${CMAKE_INSTALL_BINDIR})
SET(sysconfdir ${CMAKE_INSTALL_SYSCONFDIR})
SET(mandir ${CMAKE_INSTALL_MANDIR})

SET(rel_datadir ${CMAKE_INSTALL_DATADIR})
SET(datadir ${CMAKE_INSTALL_FULL_DATADIR})

SET(docdir ${CMAKE_INSTALL_DOCDIR})

# Comment at the top of some .in files
SET(configure_input
"This file was generated from a corresponding .in file.\
 DO NOT MANUALLY EDIT THIS FILE!")

SET(extra_completionsdir
    /usr/local/share/fish/vendor_completions.d
    CACHE STRING "Path for extra completions")

SET(extra_functionsdir
    /usr/local/share/fish/vendor_functions.d
    CACHE STRING "Path for extra functions")

SET(extra_confdir
    /usr/local/share/fish/vendor_conf.d
    CACHE STRING "Path for extra configuration")

# These are the man pages that go in system manpath; all manpages go in the fish-specific manpath.
SET(MANUALS ${CMAKE_CURRENT_BINARY_DIR}/user_doc/man/man1/fish.1
            ${CMAKE_CURRENT_BINARY_DIR}/user_doc/man/man1/fish_indent.1
            ${CMAKE_CURRENT_BINARY_DIR}/user_doc/man/man1/fish_key_reader.1)

# Determine which man page we don't want to install.
# On OS X, don't install a man page for open, since we defeat fish's open
# function on OS X.
# On other operating systems, don't install a realpath man page, as they almost all have a realpath
# command, while macOS does not.
IF(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  SET(CONDEMNED_PAGE "open.1")
ELSE()
  SET(CONDEMNED_PAGE "realpath.1")
ENDIF()

# Define a function to help us create directories.
FUNCTION(FISH_CREATE_DIRS)
  FOREACH(dir ${ARGV})
    INSTALL(DIRECTORY DESTINATION ${dir})
  ENDFOREACH(dir)
ENDFUNCTION(FISH_CREATE_DIRS)

FUNCTION(FISH_TRY_CREATE_DIRS)
  FOREACH(dir ${ARGV})
    IF(NOT IS_ABSOLUTE ${dir})
      SET(abs_dir "\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${dir}")
    ELSE()
      SET(abs_dir "\$ENV{DESTDIR}${dir}")
    ENDIF()
    INSTALL(SCRIPT CODE "EXECUTE_PROCESS(COMMAND mkdir -p ${abs_dir} OUTPUT_QUIET ERROR_QUIET)
                         EXECUTE_PROCESS(COMMAND chmod 755 ${abs_dir} OUTPUT_QUIET ERROR_QUIET)
                        ")
  ENDFOREACH()
ENDFUNCTION(FISH_TRY_CREATE_DIRS)

INSTALL(TARGETS ${PROGRAMS}
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ
                    GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        DESTINATION ${bindir})

FISH_CREATE_DIRS(${sysconfdir}/fish/conf.d ${sysconfdir}/fish/completions
    ${sysconfdir}/fish/functions)
INSTALL(FILES etc/config.fish DESTINATION ${sysconfdir}/fish/)

FISH_CREATE_DIRS(${rel_datadir}/fish ${rel_datadir}/fish/completions
                 ${rel_datadir}/fish/functions ${rel_datadir}/fish/groff
                 ${rel_datadir}/fish/man/man1 ${rel_datadir}/fish/tools
                 ${rel_datadir}/fish/tools/web_config
                 ${rel_datadir}/fish/tools/web_config/js
                 ${rel_datadir}/fish/tools/web_config/partials
                 ${rel_datadir}/fish/tools/web_config/sample_prompts)

CONFIGURE_FILE(share/__fish_build_paths.fish.in share/__fish_build_paths.fish)
INSTALL(FILES share/config.fish
              ${CMAKE_CURRENT_BINARY_DIR}/share/__fish_build_paths.fish
        DESTINATION ${rel_datadir}/fish)

# Create only the vendor directories inside the prefix (#5029 / #6508)
FISH_CREATE_DIRS(${rel_datadir}/fish/vendor_completions.d ${rel_datadir}/fish/vendor_functions.d
    ${rel_datadir}/fish/vendor_conf.d)

FISH_TRY_CREATE_DIRS(${rel_datadir}/pkgconfig)
CONFIGURE_FILE(fish.pc.in fish.pc.noversion)

ADD_CUSTOM_COMMAND(OUTPUT fish.pc
    COMMAND sed '/Version/d' fish.pc.noversion > fish.pc
    COMMAND printf "Version: " >> fish.pc
    COMMAND sed 's/FISH_BUILD_VERSION=//\;s/\"//g' ${FBVF} >> fish.pc
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${FBVF} ${CMAKE_CURRENT_BINARY_DIR}/fish.pc.noversion)

ADD_CUSTOM_TARGET(build_fish_pc ALL DEPENDS fish.pc)

INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/fish.pc
        DESTINATION ${rel_datadir}/pkgconfig)

INSTALL(DIRECTORY share/completions/
        DESTINATION ${rel_datadir}/fish/completions
        FILES_MATCHING PATTERN "*.fish")

INSTALL(DIRECTORY share/functions/
        DESTINATION ${rel_datadir}/fish/functions
        FILES_MATCHING PATTERN "*.fish")

INSTALL(DIRECTORY share/groff
        DESTINATION ${rel_datadir}/fish)

# CONDEMNED_PAGE is managed by the conditional above
# Building the man pages is optional: if sphinx isn't installed, they're not built
INSTALL(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/user_doc/man/man1/
        DESTINATION ${rel_datadir}/fish/man/man1
        FILES_MATCHING
        PATTERN "*.1"
        PATTERN ${CONDEMNED_PAGE} EXCLUDE)

INSTALL(PROGRAMS share/tools/create_manpage_completions.py share/tools/deroff.py
        DESTINATION ${rel_datadir}/fish/tools/)

INSTALL(DIRECTORY share/tools/web_config
        DESTINATION ${rel_datadir}/fish/tools/
        FILES_MATCHING
        PATTERN "*.png"
        PATTERN "*.css"
        PATTERN "*.html"
        PATTERN "*.py"
        PATTERN "*.js"
        PATTERN "*.fish")

# Building the man pages is optional: if Sphinx isn't installed, they're not built
INSTALL(FILES ${MANUALS} DESTINATION ${mandir}/man1/ OPTIONAL)
INSTALL(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/user_doc/html/ # Trailing slash is important!
        DESTINATION ${docdir} OPTIONAL)
INSTALL(FILES CHANGELOG.md DESTINATION ${docdir})

INSTALL(FILES share/lynx.lss DESTINATION ${rel_datadir}/fish/)

# These files are built by cmake/gettext.cmake, but using GETTEXT_PROCESS_PO_FILES's
# INSTALL_DESTINATION leads to them being installed as ${lang}.gmo, not fish.mo
# The ${languages} array comes from cmake/gettext.cmake
IF(GETTEXT_FOUND)
  FOREACH(lang ${languages})
    INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/${lang}.gmo DESTINATION
            ${CMAKE_INSTALL_LOCALEDIR}/${lang}/LC_MESSAGES/ RENAME fish.mo)
  ENDFOREACH()
ENDIF()

INSTALL(FILES fish.desktop DESTINATION ${rel_datadir}/applications)
INSTALL(FILES fish.png DESTINATION ${rel_datadir}/pixmaps)

# Group install targets into a InstallTargets folder
SET_PROPERTY(TARGET build_fish_pc CHECK-FISH-BUILD-VERSION-FILE
                    test_fishscript
                    test_prep tests_buildroot_target
             PROPERTY FOLDER cmake/InstallTargets)

# Make a target build_root that installs into the buildroot directory, for testing.
SET(BUILDROOT_DIR ${CMAKE_CURRENT_BINARY_DIR}/buildroot)
ADD_CUSTOM_TARGET(build_root
                  COMMAND DESTDIR=${BUILDROOT_DIR} ${CMAKE_COMMAND}
                          --build ${CMAKE_CURRENT_BINARY_DIR} --target install)
