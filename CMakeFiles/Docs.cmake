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
STRING(REPLACE ".in" "" HDR_FILES ${HDR_FILES_SRC})

# Build lexicon_filter.
ADD_CUSTOM_COMMAND(OUTPUT lexicon_filter
                   COMMAND build_tools/build_lexicon_filter.sh
                              ${CMAKE_CURRENT_SOURCE_DIR}/share/completions/
                              ${CMAKE_CURRENT_SOURCE_DIR}/share/functions/
                              < lexicon_filter.in
                              > ${CMAKE_CURRENT_BINARY_DIR}/lexicon_filter
                                && chmod a+x ${CMAKE_CURRENT_BINARY_DIR}/lexicon_filter
                  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                  DEPENDS ${COMPLETIONS_DIR_FILES} ${FUNCTIONS_DIR_FILES}
                          doc_src/commands.hdr lexicon_filter.in
                          share/functions/__fish_config_interactive.fish
                          build_tools/build_lexicon_filter.sh)

#
# commands.dr collects documentation on all commands, functions and
# builtins
#

ADD_CUSTOM_COMMAND(OUTPUT doc_src/commands.hdr
                   COMMAND build_tools/build_commands_hdr.sh ${HELP_SRC}
                               < doc_src/commands.hdr.in 
                               > ${CMAKE_CURRENT_BINARY_DIR}/doc_src/commands.hdr
                   DEPENDS ${DOC_SRC_FILES} doc_src/commands.hdr.in build_tools/build_commands_hdr.sh)

# doc.h is a compilation of the various snipptes of text used both for
# the user documentation and for internal help functions into a single
# file that can be parsed by Doxygen to generate the user
# documentation.
ADD_CUSTOM_COMMAND(OUTPUT doc.h
                   COMMAND cat  ${HDR_FILES} > ${CMAKE_CURRENT_BINARY_DIR}/doc.h
                   WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                   DEPENDS ${HDR_FILES})

ADD_CUSTOM_TARGET(doc
                  COMMAND "(cat Doxyfile.user; echo INPUT_FILTER=./lexicon_filter; echo PROJECT_NUMBER=${FISH_BUILD_VERSION} |\
                           /usr/bin/env sed 's/-.*//') | doxygen - && touch user_doc)"
                  DEPENDS ${HDR_FILES_SRC} Doxyfile.user ${DOC_SRC_FILES} doc.h $(HDR_FILES) lexicon_filter)

# doc: $(HDR_FILES_SRC) Doxyfile.user $(HTML_SRC) $(HELP_SRC) doc.h $(HDR_FILES) lexicon_filter
#   @echo "  doxygen  $(em)user_doc$(sgr0)"
#   $v (cat Doxyfile.user; echo INPUT_FILTER=./lexicon_filter; echo PROJECT_NUMBER=$(FISH_BUILD_VERSION) | $(SED) "s/-.*//") | doxygen - && touch user_doc
#   $v rm -f $(wildcard $(addprefix ./user_doc/html/,arrow*.png bc_s.png bdwn.png closed.png doc.png folder*.png ftv2*.png nav*.png open.png splitbar.png sync_*.png tab*.* doxygen.* dynsections.js jquery.js pages.html))