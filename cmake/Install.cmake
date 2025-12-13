set(CMAKE_INSTALL_MESSAGE NEVER)

set(prefix ${CMAKE_INSTALL_PREFIX})
set(bindir ${CMAKE_INSTALL_BINDIR})
set(sysconfdir ${CMAKE_INSTALL_SYSCONFDIR})
set(mandir ${CMAKE_INSTALL_MANDIR})

set(datadir ${CMAKE_INSTALL_FULL_DATADIR})
file(RELATIVE_PATH rel_datadir ${CMAKE_INSTALL_PREFIX} ${datadir})

set(docdir ${CMAKE_INSTALL_DOCDIR})

set(rel_completionsdir "fish/vendor_completions.d")
set(rel_functionsdir "fish/vendor_functions.d")
set(rel_confdir "fish/vendor_conf.d")

set(extra_completionsdir
    "${datadir}/${rel_completionsdir}"
    CACHE STRING "Path for extra completions")

set(extra_functionsdir
    "${datadir}/${rel_functionsdir}"
    CACHE STRING "Path for extra functions")

set(extra_confdir
    "${datadir}/${rel_confdir}"
    CACHE STRING "Path for extra configuration")


# These are the man pages that go in system manpath; all manpages go in the fish-specific manpath.
set(MANUALS ${CMAKE_CURRENT_BINARY_DIR}/user_doc/man/man1/fish.1
            ${CMAKE_CURRENT_BINARY_DIR}/user_doc/man/man1/fish_indent.1
            ${CMAKE_CURRENT_BINARY_DIR}/user_doc/man/man1/fish_key_reader.1
            ${CMAKE_CURRENT_BINARY_DIR}/user_doc/man/man1/fish-doc.1
            ${CMAKE_CURRENT_BINARY_DIR}/user_doc/man/man1/fish-tutorial.1
            ${CMAKE_CURRENT_BINARY_DIR}/user_doc/man/man1/fish-language.1
            ${CMAKE_CURRENT_BINARY_DIR}/user_doc/man/man1/fish-interactive.1
            ${CMAKE_CURRENT_BINARY_DIR}/user_doc/man/man1/fish-terminal-compatibility.1
            ${CMAKE_CURRENT_BINARY_DIR}/user_doc/man/man1/fish-completions.1
            ${CMAKE_CURRENT_BINARY_DIR}/user_doc/man/man1/fish-prompt-tutorial.1
            ${CMAKE_CURRENT_BINARY_DIR}/user_doc/man/man1/fish-for-bash-users.1
            ${CMAKE_CURRENT_BINARY_DIR}/user_doc/man/man1/fish-faq.1
)

# Determine which man page we don't want to install.
# On OS X, don't install a man page for open, since we defeat fish's open
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

install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/fish
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ
                    GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        DESTINATION ${bindir})

if(NOT IS_ABSOLUTE ${bindir})
  set(abs_bindir "\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${bindir}")
else()
  set(abs_bindir "\$ENV{DESTDIR}${bindir}")
endif()
install(CODE "file(CREATE_LINK ${abs_bindir}/fish ${abs_bindir}/fish_indent)")
install(CODE "file(CREATE_LINK ${abs_bindir}/fish ${abs_bindir}/fish_key_reader)")

fish_create_dirs(${sysconfdir}/fish/conf.d ${sysconfdir}/fish/completions
    ${sysconfdir}/fish/functions)
install(FILES etc/config.fish DESTINATION ${sysconfdir}/fish/)

fish_create_dirs(${rel_datadir}/fish ${rel_datadir}/fish/completions
                 ${rel_datadir}/fish/functions ${rel_datadir}/fish/groff
                 ${rel_datadir}/fish/man/man1 ${rel_datadir}/fish/tools
                 ${rel_datadir}/fish/tools/web_config
                 ${rel_datadir}/fish/tools/web_config/js
                 ${rel_datadir}/fish/prompts
                 ${rel_datadir}/fish/themes
                 )

configure_file(share/__fish_build_paths.fish.in share/__fish_build_paths.fish)
install(FILES share/config.fish
              ${CMAKE_CURRENT_BINARY_DIR}/share/__fish_build_paths.fish
        DESTINATION ${rel_datadir}/fish)

# Create only the vendor directories inside the prefix (#5029 / #6508)
fish_create_dirs(${rel_datadir}/fish/vendor_completions.d ${rel_datadir}/fish/vendor_functions.d
    ${rel_datadir}/fish/vendor_conf.d)

fish_try_create_dirs(${rel_datadir}/pkgconfig)
configure_file(fish.pc.in fish.pc.noversion @ONLY)

add_custom_command(OUTPUT fish.pc
    COMMAND sed '/Version/d' fish.pc.noversion > fish.pc
    COMMAND printf "Version: " >> fish.pc
    COMMAND cat ${FBVF} >> fish.pc
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    DEPENDS CHECK-FISH-BUILD-VERSION-FILE ${CMAKE_CURRENT_BINARY_DIR}/fish.pc.noversion)

add_custom_target(build_fish_pc ALL DEPENDS fish.pc)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/fish.pc
        DESTINATION ${rel_datadir}/pkgconfig)

install(DIRECTORY share/completions/
        DESTINATION ${rel_datadir}/fish/completions
        FILES_MATCHING PATTERN "*.fish")

install(DIRECTORY share/functions/
        DESTINATION ${rel_datadir}/fish/functions
        FILES_MATCHING PATTERN "*.fish")

install(DIRECTORY share/groff
        DESTINATION ${rel_datadir}/fish)

# CONDEMNED_PAGE is managed by the conditional above
# Building the man pages is optional: if sphinx isn't installed, they're not built
install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/user_doc/man/man1/
        DESTINATION ${rel_datadir}/fish/man/man1
        FILES_MATCHING
        PATTERN "*.1"
        PATTERN ${CONDEMNED_PAGE} EXCLUDE)

install(PROGRAMS share/tools/create_manpage_completions.py
        DESTINATION ${rel_datadir}/fish/tools/)

install(DIRECTORY share/tools/web_config
        DESTINATION ${rel_datadir}/fish/tools/
        FILES_MATCHING
        PATTERN "*.png"
        PATTERN "*.css"
        PATTERN "*.html"
        PATTERN "*.py"
        PATTERN "*.js"
        PATTERN "*.theme"
        PATTERN "*.fish")

# Building the man pages is optional: if Sphinx isn't installed, they're not built
install(FILES ${MANUALS} DESTINATION ${mandir}/man1/ OPTIONAL)
install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/user_doc/html/ # Trailing slash is important!
        DESTINATION ${docdir} OPTIONAL)
install(FILES CHANGELOG.rst DESTINATION ${docdir})

# Group install targets into a InstallTargets folder
set_property(TARGET build_fish_pc CHECK-FISH-BUILD-VERSION-FILE
             PROPERTY FOLDER cmake/InstallTargets)

# Make a target build_root that installs into the buildroot directory, for testing.
set(BUILDROOT_DIR ${CMAKE_CURRENT_BINARY_DIR}/buildroot)
add_custom_target(build_root
                  COMMAND DESTDIR=${BUILDROOT_DIR} ${CMAKE_COMMAND}
                          --build ${CMAKE_CURRENT_BINARY_DIR} --target install)
