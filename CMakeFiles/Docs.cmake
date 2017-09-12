# Files in ./share/completions/
FILE(GLOB COMPLETIONS_DIR_FILES share/completions/*.fish)

# Files in ./share/functions/
FILE(GLOB FUNCTIONS_DIR_FILES share/functions/*.fish)

# Build lexicon_filter
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
