# -DLOCALEDIR="${CMAKE_INSTALL_FULL_LOCALEDIR}"
# -DPREFIX=L"${CMAKE_INSTALL_PREFIX}"
# -DDATADIR=L"${CMAKE_INSTALL_FULL_DATADIR}"
# -DSYSCONFDIR=L"${CMAKE_INSTALL_FULL_SYSCONFDIR}"
# -DBINDIR=L"${CMAKE_INSTALL_FULL_BINDIR}"
# -DDOCDIR=L"${CMAKE_INSTALL_FULL_DOCDIR}")

set(CMAKE_INSTALL_MESSAGE NEVER)

set(PROGRAMS ghoti ghoti_indent ghoti_key_reader)

set(prefix ${CMAKE_INSTALL_PREFIX})
set(bindir ${CMAKE_INSTALL_BINDIR})
set(sysconfdir ${CMAKE_INSTALL_SYSCONFDIR})
set(mandir ${CMAKE_INSTALL_MANDIR})

set(datadir ${CMAKE_INSTALL_FULL_DATADIR})
file(RELATIVE_PATH rel_datadir ${CMAKE_INSTALL_PREFIX} ${datadir})

set(docdir ${CMAKE_INSTALL_DOCDIR})

# Comment at the top of some .in files
set(configure_input
"This file was generated from a corresponding .in file.\
 DO NOT MANUALLY EDIT THIS FILE!")

set(rel_completionsdir "ghoti/vendor_completions.d")
set(rel_functionsdir "ghoti/vendor_functions.d")
set(rel_confdir "ghoti/vendor_conf.d")

set(extra_completionsdir
    "${datadir}/${rel_completionsdir}"
    CACHE STRING "Path for extra completions")

set(extra_functionsdir
    "${datadir}/${rel_functionsdir}"
    CACHE STRING "Path for extra functions")

set(extra_confdir
    "${datadir}/${rel_confdir}"
    CACHE STRING "Path for extra configuration")

# These are the man pages that go in system manpath; all manpages go in the ghoti-specific manpath.
set(MANUALS ${CMAKE_CURRENT_BINARY_DIR}/user_doc/man/man1/ghoti.1
            ${CMAKE_CURRENT_BINARY_DIR}/user_doc/man/man1/ghoti_indent.1
            ${CMAKE_CURRENT_BINARY_DIR}/user_doc/man/man1/ghoti_key_reader.1)

# Determine which man page we don't want to install.
# On OS X, don't install a man page for open, since we defeat ghoti's open
# function on OS X.
# On other operating systems, don't install a realpath man page, as they almost all have a realpath
# command, while macOS does not.
if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  set(CONDEMNED_PAGE "open.1")
else()
  set(CONDEMNED_PAGE "realpath.1")
endif()

# Define a function to help us create directories.
function(FISH_CREATE_DIRS)
  foreach(dir ${ARGV})
    install(DIRECTORY DESTINATION ${dir})
  endforeach(dir)
endfunction(FISH_CREATE_DIRS)

function(FISH_TRY_CREATE_DIRS)
  foreach(dir ${ARGV})
    if(NOT IS_ABSOLUTE ${dir})
      set(abs_dir "\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${dir}")
    else()
      set(abs_dir "\$ENV{DESTDIR}${dir}")
    endif()
    install(SCRIPT CODE "EXECUTE_PROCESS(COMMAND mkdir -p ${abs_dir} OUTPUT_QUIET ERROR_QUIET)
                         execute_process(COMMAND chmod 755 ${abs_dir} OUTPUT_QUIET ERROR_QUIET)
                        ")
  endforeach()
endfunction(FISH_TRY_CREATE_DIRS)

install(TARGETS ${PROGRAMS}
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ
                    GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        DESTINATION ${bindir})

ghoti_create_dirs(${sysconfdir}/ghoti/conf.d ${sysconfdir}/ghoti/completions
    ${sysconfdir}/ghoti/functions)
install(FILES etc/config.ghoti DESTINATION ${sysconfdir}/ghoti/)

ghoti_create_dirs(${rel_datadir}/ghoti ${rel_datadir}/ghoti/completions
                 ${rel_datadir}/ghoti/functions ${rel_datadir}/ghoti/groff
                 ${rel_datadir}/ghoti/man/man1 ${rel_datadir}/ghoti/tools
                 ${rel_datadir}/ghoti/tools/web_config
                 ${rel_datadir}/ghoti/tools/web_config/js
                 ${rel_datadir}/ghoti/tools/web_config/partials
                 ${rel_datadir}/ghoti/tools/web_config/sample_prompts
                 ${rel_datadir}/ghoti/tools/web_config/themes
                 )

configure_file(share/__ghoti_build_paths.ghoti.in share/__ghoti_build_paths.ghoti)
install(FILES share/config.ghoti
              ${CMAKE_CURRENT_BINARY_DIR}/share/__ghoti_build_paths.ghoti
        DESTINATION ${rel_datadir}/ghoti)

# Create only the vendor directories inside the prefix (#5029 / #6508)
ghoti_create_dirs(${rel_datadir}/ghoti/vendor_completions.d ${rel_datadir}/ghoti/vendor_functions.d
    ${rel_datadir}/ghoti/vendor_conf.d)

ghoti_try_create_dirs(${rel_datadir}/pkgconfig)
configure_file(ghoti.pc.in ghoti.pc.noversion @ONLY)

add_custom_command(OUTPUT ghoti.pc
    COMMAND sed '/Version/d' ghoti.pc.noversion > ghoti.pc
    COMMAND printf "Version: " >> ghoti.pc
    COMMAND sed 's/FISH_BUILD_VERSION=//\;s/\"//g' ${FBVF} >> ghoti.pc
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    DEPENDS CHECK-FISH-BUILD-VERSION-FILE ${CMAKE_CURRENT_BINARY_DIR}/ghoti.pc.noversion)

add_custom_target(build_ghoti_pc ALL DEPENDS ghoti.pc)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ghoti.pc
        DESTINATION ${rel_datadir}/pkgconfig)

install(DIRECTORY share/completions/
        DESTINATION ${rel_datadir}/ghoti/completions
        FILES_MATCHING PATTERN "*.ghoti")

install(DIRECTORY share/functions/
        DESTINATION ${rel_datadir}/ghoti/functions
        FILES_MATCHING PATTERN "*.ghoti")

install(DIRECTORY share/groff
        DESTINATION ${rel_datadir}/ghoti)

# CONDEMNED_PAGE is managed by the conditional above
# Building the man pages is optional: if sphinx isn't installed, they're not built
install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/user_doc/man/man1/
        DESTINATION ${rel_datadir}/ghoti/man/man1
        FILES_MATCHING
        PATTERN "*.1"
        PATTERN ${CONDEMNED_PAGE} EXCLUDE)

install(PROGRAMS share/tools/create_manpage_completions.py share/tools/deroff.py
        DESTINATION ${rel_datadir}/ghoti/tools/)

install(DIRECTORY share/tools/web_config
        DESTINATION ${rel_datadir}/ghoti/tools/
        FILES_MATCHING
        PATTERN "*.png"
        PATTERN "*.css"
        PATTERN "*.html"
        PATTERN "*.py"
        PATTERN "*.js"
        PATTERN "*.theme"
        PATTERN "*.ghoti")

# Building the man pages is optional: if Sphinx isn't installed, they're not built
install(FILES ${MANUALS} DESTINATION ${mandir}/man1/ OPTIONAL)
install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/user_doc/html/ # Trailing slash is important!
        DESTINATION ${docdir} OPTIONAL)
install(FILES CHANGELOG.rst DESTINATION ${docdir})

# These files are built by cmake/gettext.cmake, but using GETTEXT_PROCESS_PO_FILES's
# INSTALL_DESTINATION leads to them being installed as ${lang}.gmo, not ghoti.mo
# The ${languages} array comes from cmake/gettext.cmake
if(GETTEXT_FOUND)
  foreach(lang ${languages})
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${lang}.gmo DESTINATION
            ${CMAKE_INSTALL_LOCALEDIR}/${lang}/LC_MESSAGES/ RENAME ghoti.mo)
  endforeach()
endif()

if (NOT APPLE)
    install(FILES ghoti.desktop DESTINATION ${rel_datadir}/applications)
    install(FILES ${SPHINX_SRC_DIR}/python_docs_theme/static/ghoti.png DESTINATION ${rel_datadir}/pixmaps)
endif()

# Group install targets into a InstallTargets folder
set_property(TARGET build_ghoti_pc CHECK-FISH-BUILD-VERSION-FILE
                    tests_buildroot_target
             PROPERTY FOLDER cmake/InstallTargets)

# Make a target build_root that installs into the buildroot directory, for testing.
set(BUILDROOT_DIR ${CMAKE_CURRENT_BINARY_DIR}/buildroot)
add_custom_target(build_root
                  COMMAND DESTDIR=${BUILDROOT_DIR} ${CMAKE_COMMAND}
                          --build ${CMAKE_CURRENT_BINARY_DIR} --target install)
