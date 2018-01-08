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

# Comment at the top of some .in files
SET(configure_input
"This file was generated from a corresponding .in file.\
 DO NOT MANUALLY EDIT THIS FILE!")

SET(extra_completionsdir
    ${rel_datadir}/fish/vendor_completions.d
    CACHE STRING "Path for extra completions")

SET(extra_functionsdir
    ${rel_datadir}/fish/vendor_functions.d
    CACHE STRING "Path for extra completions")

SET(extra_confdir
    ${rel_datadir}/fish/vendor_conf.d
    CACHE STRING "Path for extra configuration")

# These are the man pages that go in system manpath.
SET(MANUALS ${CMAKE_CURRENT_BINARY_DIR}/share/man/man1/fish.1
            ${CMAKE_CURRENT_BINARY_DIR}/share/man/man1/fish_indent.1
            ${CMAKE_CURRENT_BINARY_DIR}/share/man/man1/fish_key_reader.1)

# These are the manpages that go in fish-specific manpath.
FILE(GLOB HELP_MANPAGES share/man/man1/*.1)

# Determine which man pages we don't want to install.
# On OS X, don't install a man page for open, since we defeat fish's open
# function on OS X.
IF(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  LIST(REMOVE_ITEM HELP_MANPAGES share/man/man1/open.1)
ENDIF()

# Define a function to help us create directories.
FUNCTION(FISH_CREATE_DIRS)
  FOREACH(dir ${ARGV})
    INSTALL(DIRECTORY DESTINATION ${dir})
  ENDFOREACH(dir)
ENDFUNCTION(FISH_CREATE_DIRS)

# $v $(INSTALL) -m 755 -d $(DESTDIR)$(bindir)
# $v for i in $(PROGRAMS); do\
#   $(INSTALL) -m 755 $$i $(DESTDIR)$(bindir);\
#   echo " Installing $(bo)$$i$(sgr0)";\
#   true ;\
# done;

INSTALL(TARGETS ${PROGRAMS}
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ
                    GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        DESTINATION ${bindir})

# $v $(INSTALL) -m 755 -d $(DESTDIR)$(sysconfdir)/fish
# $v $(INSTALL) -m 755 -d $(DESTDIR)$(sysconfdir)/fish/conf.d
# $v $(INSTALL) -m 644 etc/config.fish $(DESTDIR)$(sysconfdir)/fish/
FISH_CREATE_DIRS(${sysconfdir}/fish/conf.d)
INSTALL(FILES etc/config.fish DESTINATION ${sysconfdir}/fish/)

# $v $(INSTALL) -m 755 -d $(DESTDIR)$(datadir)/fish
# $v $(INSTALL) -m 755 -d $(DESTDIR)$(datadir)/fish/completions
# $v $(INSTALL) -m 755 -d $(DESTDIR)$(datadir)/fish/functions
# $v $(INSTALL) -m 755 -d $(DESTDIR)$(datadir)/fish/groff
# $v $(INSTALL) -m 755 -d $(DESTDIR)$(datadir)/fish/man/man1
# $v $(INSTALL) -m 755 -d $(DESTDIR)$(datadir)/fish/tools
# $v $(INSTALL) -m 755 -d $(DESTDIR)$(datadir)/fish/tools/web_config
# $v $(INSTALL) -m 755 -d $(DESTDIR)$(datadir)/fish/tools/web_config/js
# $v $(INSTALL) -m 755 -d $(DESTDIR)$(datadir)/fish/tools/web_config/partials
# $v $(INSTALL) -m 755 -d $(DESTDIR)$(datadir)/fish/tools/web_config/sample_prompts
FISH_CREATE_DIRS(${rel_datadir}/fish ${rel_datadir}/fish/completions
                 ${rel_datadir}/fish/functions ${rel_datadir}/fish/groff
                 ${rel_datadir}/fish/man/man1 ${rel_datadir}/fish/tools
                 ${rel_datadir}/fish/tools/web_config
                 ${rel_datadir}/fish/tools/web_config/js
                 ${rel_datadir}/fish/tools/web_config/partials
                 ${rel_datadir}/fish/tools/web_config/sample_prompts)

# $v $(INSTALL) -m 644 share/config.fish $(DESTDIR)$(datadir)/fish/
# $v $(INSTALL) -m 644 share/__fish_build_paths.fish    $(DESTDIR)$(datadir)/fish/
CONFIGURE_FILE(share/__fish_build_paths.fish.in share/__fish_build_paths.fish)
INSTALL(FILES share/config.fish
              ${CMAKE_CURRENT_BINARY_DIR}/share/__fish_build_paths.fish
        DESTINATION ${rel_datadir}/fish)

# $v $(INSTALL) -m 755 -d $(DESTDIR)$(datadir)/pkgconfig
# @echo "Creating placeholder vendor/'extra_' directories"
# -$v $(INSTALL) -m 755 -d $(DESTDIR)$(extra_completionsdir)
# -$v $(INSTALL) -m 755 -d $(DESTDIR)$(extra_functionsdir)
# -$v $(INSTALL) -m 755 -d $(DESTDIR)$(extra_confdir)
FISH_CREATE_DIRS(${rel_datadir}/pkgconfig ${extra_completionsdir}
                 ${extra_functionsdir} ${extra_confdir})

# @echo "Installing pkgconfig file"
# $v $(INSTALL) -m 644 fish.pc $(DESTDIR)$(datadir)/pkgconfig
CONFIGURE_FILE(fish.pc.in fish.pc.noversion)
ADD_CUSTOM_COMMAND(OUTPUT fish.pc
  COMMAND awk -v `cat ${FBVF}` '/^Version:/ {$$0=$$0 FISH_BUILD_VERSION} 1' fish.pc.noversion  > fish.pc
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  DEPENDS ${FBVF} ${CMAKE_CURRENT_BINARY_DIR}/fish.pc.noversion)

INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/fish.pc
        DESTINATION ${rel_datadir}/pkgconfig)

# @echo "Installing the $(bo)fish completion library$(sgr0)...";
# $v $(INSTALL) -m 644 $(COMPLETIONS_DIR_FILES:%='%') $(DESTDIR)$(datadir)/fish/completions/
INSTALL(DIRECTORY share/completions/
        DESTINATION ${rel_datadir}/fish/completions
        FILES_MATCHING PATTERN "*.fish")

# @echo "Installing $(bo)fish functions$(sgr0)";
# $v $(INSTALL) -m 644 $(FUNCTIONS_DIR_FILES:%='%') $(DESTDIR)$(datadir)/fish/functions/
INSTALL(DIRECTORY share/functions/
        DESTINATION ${rel_datadir}/fish/functions
        FILES_MATCHING PATTERN "*.fish")

# @echo "Installing $(bo)man pages$(sgr0)";
# $v $(INSTALL) -m 644 share/groff/* $(DESTDIR)$(datadir)/fish/groff/
INSTALL(DIRECTORY share/groff
        DESTINATION ${rel_datadir}/fish)

# $v test -z "$(wildcard share/man/man1/*.1)" || $(INSTALL) -m 644 $(filter-out $(addprefix share/man/man1/, $(CONDEMNED_PAGES)), $(wildcard share/man/man1/*.1)) $(DESTDIR)$(datadir)/fish/man/man1/
INSTALL(FILES ${HELP_MANPAGES}
        DESTINATION ${rel_datadir}/fish/man/man1)

# @echo "Installing helper tools";
# $v $(INSTALL) -m 755 share/tools/*.py $(DESTDIR)$(datadir)/fish/tools/
INSTALL(PROGRAMS share/tools/create_manpage_completions.py share/tools/deroff.py
        DESTINATION ${rel_datadir}/fish/tools/)

# $v $(INSTALL) -m 644 share/tools/web_config/*.* $(DESTDIR)$(datadir)/fish/tools/web_config/
# $v $(INSTALL) -m 644 share/tools/web_config/js/*.* $(DESTDIR)$(datadir)/fish/tools/web_config/js/
# $v $(INSTALL) -m 644 share/tools/web_config/partials/* $(DESTDIR)$(datadir)/fish/tools/web_config/partials/
# $v $(INSTALL) -m 644 share/tools/web_config/sample_prompts/*.fish $(DESTDIR)$(datadir)/fish/tools/web_config/sample_prompts/
# $v $(INSTALL) -m 755 share/tools/web_config/*.py $(DESTDIR)$(datadir)/fish/tools/web_config/

INSTALL(DIRECTORY share/tools/web_config
        DESTINATION ${rel_datadir}/fish/tools/
        FILES_MATCHING
        PATTERN "*.png"
        PATTERN "*.css"
        PATTERN "*.html"
        PATTERN "*.py"
        PATTERN "*.js"
        PATTERN "*.fish")

#$v test -z "$(wildcard share/man/man1/*.1)" || $(INSTALL) -m 644 $(filter-out $(addprefix share/man/man1/, $(CONDEMNED_PAGES)), $(wildcard share/man/man1/*.1)) #$(DESTDIR)$(datadir)/fish/man/man1/
#TODO: CONDEMNED_PAGES
# Building the man pages is optional: if doxygen isn't installed, they're not built
INSTALL(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/share/man/
        DESTINATION ${rel_datadir}/fish/man/ OPTIONAL)

# @echo "Installing more man pages";
# $v $(INSTALL) -m 755 -d $(DESTDIR)$(mandir)/man1;
# $v for i in $(MANUALS); do \
#   $(INSTALL) -m 644 $$i $(DESTDIR)$(mandir)/man1/; \
#   true; \
# done;
# Building the man pages is optional: if doxygen isn't installed, they're not built
INSTALL(FILES ${MANUALS} DESTINATION ${mandir}/man1/ OPTIONAL)

# $v $(INSTALL) -m 644 share/lynx.lss $(DESTDIR)$(datadir)/fish/
INSTALL(FILES share/lynx.lss DESTINATION ${rel_datadir}/fish/)

# Group install targets into a InstallTargets folder
SET_PROPERTY(TARGET CHECK-FISH-BUILD-VERSION-FILE
                    test_invocation test_fishscript
                    test_prep tests_buildroot_target
                    build_lexicon_filter
             PROPERTY FOLDER cmake/InstallTargets)

# Make a target build_root that installs into the buildroot directory, for testing.
SET(BUILDROOT_DIR ${CMAKE_CURRENT_BINARY_DIR}/buildroot)
ADD_CUSTOM_TARGET(build_root
                  COMMAND DESTDIR=${BUILDROOT_DIR} ${CMAKE_COMMAND}
                          --build ${CMAKE_CURRENT_BINARY_DIR} --target install)
