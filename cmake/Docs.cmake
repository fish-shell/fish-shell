FIND_PACKAGE(Doxygen 1.8.7)

INCLUDE(FeatureSummary)

IF(DOXYGEN_FOUND)
    OPTION(BUILD_DOCS "build documentation (requires Doxygen)" ON)
ELSE(DOXYGEN_FOUND)
    OPTION(BUILD_DOCS "build documentation (requires Doxygen)" OFF)
ENDIF(DOXYGEN_FOUND)

IF(BUILD_DOCS AND NOT DOXYGEN_FOUND)
    MESSAGE(FATAL_ERROR "build documentation selected, but Doxygen could not be found")
ENDIF()

IF(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/user_doc/html
   AND IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/share/man/man1)
    SET(HAVE_PREBUILT_DOCS TRUE)
ELSE()
    SET(HAVE_PREBUILT_DOCS FALSE)
ENDIF()

IF(BUILD_DOCS OR HAVE_PREBUILT_DOCS)
    SET(INSTALL_DOCS ON)
ELSE()
    SET(INSTALL_DOCS OFF)
ENDIF()

ADD_FEATURE_INFO(Documentation INSTALL_DOCS "user manual and documentation")

IF(BUILD_DOCS)
    # Files in ./share/completions/
    FILE(GLOB COMPLETIONS_DIR_FILES share/completions/*.fish)

    # Files in ./share/functions/
    FILE(GLOB FUNCTIONS_DIR_FILES share/functions/*.fish)

    # Files in doc_src
    FILE(GLOB DOC_SRC_FILES doc_src/*)

    # .txt files in doc_src
    FILE(GLOB HELP_SRC doc_src/*.txt)

    # These files are the source files, they contain a few @FOO@-style substitutions.
    # Note that this order defines the order that they appear in the documentation.
    SET(HDR_FILES_SRC doc_src/index.hdr.in doc_src/tutorial.hdr doc_src/design.hdr
                      doc_src/license.hdr doc_src/commands.hdr.in doc_src/faq.hdr)

    # These are the generated result files.
    STRING(REPLACE ".in" "" HDR_FILES "${HDR_FILES_SRC}")

    # Header files except for index.hdr
    SET(HDR_FILES_NO_INDEX ${HDR_FILES})
    LIST(REMOVE_ITEM HDR_FILES_NO_INDEX doc_src/index.hdr)

    # Copy doc_src files
    FILE(COPY ${DOC_SRC_FILES} DESTINATION doc_src)

    # Build lexicon_filter.
    ADD_CUSTOM_COMMAND(OUTPUT lexicon_filter
                       COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/build_tools/build_lexicon_filter.sh
                                  ${CMAKE_CURRENT_SOURCE_DIR}/share/functions/
                                  ${CMAKE_CURRENT_SOURCE_DIR}/share/completions/
                                  ${CMAKE_CURRENT_SOURCE_DIR}/lexicon_filter.in
                                  ${SED}
                                  > ${CMAKE_CURRENT_BINARY_DIR}/lexicon_filter
                                    && chmod a+x ${CMAKE_CURRENT_BINARY_DIR}/lexicon_filter
                      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                      DEPENDS ${FUNCTIONS_DIR_FILES} ${COMPLETIONS_DIR_FILES}
                              doc_src/commands.hdr ${CMAKE_CURRENT_SOURCE_DIR}/lexicon_filter.in
                              share/functions/__fish_config_interactive.fish
                              build_tools/build_lexicon_filter.sh command_list_toc.txt)

    # Other targets should depend on this target, otherwise the lexicon
    # filter can be built twice.
    ADD_CUSTOM_TARGET(build_lexicon_filter DEPENDS lexicon_filter)

    #
    # commands.hdr collects documentation on all commands, functions and
    # builtins
    #
    FILE(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/doc_src)
    ADD_CUSTOM_COMMAND(OUTPUT doc_src/commands.hdr command_list_toc.txt
                       WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                       COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/build_tools/build_commands_hdr.sh ${HELP_SRC}
                                   < doc_src/commands.hdr.in
                                   > ${CMAKE_CURRENT_BINARY_DIR}/doc_src/commands.hdr
                       DEPENDS ${HELP_SRC}
                               ${CMAKE_CURRENT_SOURCE_DIR}/doc_src/commands.hdr.in
                               ${CMAKE_CURRENT_SOURCE_DIR}/build_tools/build_commands_hdr.sh)

    # doc.h is a compilation of the various snippets of text used both for
    # the user documentation and for internal help functions into a single
    # file that can be parsed by Doxygen to generate the user
    # documentation.
    ADD_CUSTOM_COMMAND(OUTPUT doc.h
                       COMMAND cat ${HDR_FILES} > ${CMAKE_CURRENT_BINARY_DIR}/doc.h
                       DEPENDS ${HDR_FILES})

    # toc.txt: $(HDR_FILES:index.hdr=index.hdr.in) build_tools/build_toc_txt.sh | show-SED
    #   FISH_BUILD_VERSION=${FISH_BUILD_VERSION} build_tools/build_toc_txt.sh \
    #     $(HDR_FILES:index.hdr=index.hdr.in) > toc.txt
    # Note we would like to add doc_src/index.hdr.in as a dependency but CMake replaces this with
    # doc_src/index.hdr; CMake bug?
    ADD_CUSTOM_COMMAND(OUTPUT toc.txt
                       COMMAND env `cat ${FBVF} | tr -d '\"'` ${CMAKE_CURRENT_SOURCE_DIR}/build_tools/build_toc_txt.sh
                               doc_src/index.hdr.in ${HDR_FILES_NO_INDEX}
                               > ${CMAKE_CURRENT_BINARY_DIR}/toc.txt
                       DEPENDS ${CFBVF} ${HDR_FILES_NO_INDEX})

    # doc_src/index.hdr: toc.txt doc_src/index.hdr.in | show-AWK
    # @echo "  AWK CAT  $(em)$@$(sgr0)"
    # $v cat $@.in | $(AWK) '{if ($$0 ~ /@toc@/){ system("cat toc.txt");} else{ print $$0;}}' >$@
    ADD_CUSTOM_COMMAND(OUTPUT doc_src/index.hdr
                       COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/build_tools/build_index_hdr.sh toc.txt
                               < doc_src/index.hdr.in
                               > ${CMAKE_CURRENT_BINARY_DIR}/doc_src/index.hdr
                       DEPENDS toc.txt)

    # doc: $(HDR_FILES_SRC) Doxyfile.user $(HTML_SRC) $(HELP_SRC) doc.h $(HDR_FILES) lexicon_filter
    #   @echo "  doxygen  $(em)user_doc$(sgr0)"
    #   $v (cat Doxyfile.user; echo INPUT_FILTER=./lexicon_filter; echo PROJECT_NUMBER=$(FISH_BUILD_VERSION) | $(SED) "s/-.*//") | doxygen - && touch user_doc
    #   $v rm -f $(wildcard $(addprefix ./user_doc/html/,arrow*.png bc_s.png bdwn.png closed.png doc.png folder*.png ftv2*.png nav*.png open.png splitbar.png sync_*.png tab*.* doxygen.* dynsections.js jquery.js pages.html))
    ADD_CUSTOM_TARGET(doc ALL
                      COMMAND env `cat ${FBVF}`
                              ${CMAKE_CURRENT_SOURCE_DIR}/build_tools/build_user_doc.sh
                              ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.user ./lexicon_filter
                      DEPENDS ${CFBVF} Doxyfile.user ${DOC_SRC_FILES} doc.h ${HDR_FILES} build_lexicon_filter command_list_toc.txt)

    ADD_CUSTOM_COMMAND(OUTPUT share/man/
                       COMMAND env `cat ${FBVF} | tr -d '\"' `
                               INPUT_FILTER=lexicon_filter ${CMAKE_CURRENT_SOURCE_DIR}/build_tools/build_documentation.sh ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.help doc_src ./share
                       DEPENDS ${CFBVF} ${HELP_SRC} build_lexicon_filter)

    ADD_CUSTOM_TARGET(BUILD_MANUALS ALL DEPENDS share/man/)

    # Group docs targets into a DocsTargets folder
    SET_PROPERTY(TARGET doc BUILD_MANUALS build_lexicon_filter
                 PROPERTY FOLDER cmake/DocTargets)
ELSEIF(HAVE_PREBUILT_DOCS)
    IF(NOT CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_CURRENT_BINARY_DIR)
        # Out of tree build - link the prebuilt documentation to the build tree
        ADD_CUSTOM_TARGET(link_doc ALL)
        ADD_CUSTOM_COMMAND(TARGET link_doc
            COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_CURRENT_SOURCE_DIR}/share/man ${CMAKE_CURRENT_BINARY_DIR}/share/man
            POST_BUILD)
        ADD_CUSTOM_COMMAND(TARGET link_doc
            COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_CURRENT_SOURCE_DIR}/user_doc ${CMAKE_CURRENT_BINARY_DIR}/user_doc
            POST_BUILD)
    ENDIF()
ENDIF(BUILD_DOCS)
