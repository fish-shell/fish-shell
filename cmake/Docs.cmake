FIND_PROGRAM(SPHINX_EXECUTABLE NAMES sphinx-build
    HINTS
    $ENV{SPHINX_DIR}
    PATH_SUFFIXES bin
    DOC "Sphinx documentation generator")

INCLUDE(FeatureSummary)

SET(SPHINX_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/sphinx_doc_src")
SET(SPHINX_ROOT_DIR "${CMAKE_CURRENT_BINARY_DIR}/user_doc")
SET(SPHINX_BUILD_DIR "${SPHINX_ROOT_DIR}/build")
SET(SPHINX_CACHE_DIR "${SPHINX_ROOT_DIR}/doctrees")
SET(SPHINX_HTML_DIR "${SPHINX_ROOT_DIR}/html")
SET(SPHINX_MANPAGE_DIR "${SPHINX_ROOT_DIR}/man")

# sphinx-docs uses fish_indent for highlighting.
# Prepend the output dir of fish_indent to PATH.
ADD_CUSTOM_TARGET(sphinx-docs
    mkdir -p ${SPHINX_HTML_DIR}/_static/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SPHINX_SRC_DIR}/_static/pygments.css ${SPHINX_HTML_DIR}/_static/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SPHINX_SRC_DIR}/_static/custom.css ${SPHINX_HTML_DIR}/_static/
    COMMAND env PATH="$<TARGET_FILE_DIR:fish_indent>:$$PATH"
        ${SPHINX_EXECUTABLE}
        -q -b html
        -c "${SPHINX_SRC_DIR}"
        -d "${SPHINX_CACHE_DIR}"
        "${SPHINX_SRC_DIR}"
        "${SPHINX_HTML_DIR}"
    DEPENDS sphinx_doc_src/fish_indent_lexer.py fish_indent
    COMMENT "Building HTML documentation with Sphinx")

# sphinx-manpages needs the fish_indent binary for the version number
ADD_CUSTOM_TARGET(sphinx-manpages
    env PATH="$<TARGET_FILE_DIR:fish_indent>:$$PATH"
        ${SPHINX_EXECUTABLE}
        -q -b man
        -c "${SPHINX_SRC_DIR}"
        -d "${SPHINX_CACHE_DIR}"
        "${SPHINX_SRC_DIR}"
        # TODO: This only works if we only have section 1 manpages.
        "${SPHINX_MANPAGE_DIR}/man1"
    DEPENDS fish_indent
    COMMENT "Building man pages with Sphinx")

IF(SPHINX_EXECUTABLE)
    OPTION(BUILD_DOCS "build documentation (requires Sphinx)" ON)
ELSE(SPHINX_EXECUTABLE)
    OPTION(BUILD_DOCS "build documentation (requires Sphinx)" OFF)
ENDIF(SPHINX_EXECUTABLE)

IF(BUILD_DOCS AND NOT SPHINX_EXECUTABLE)
    MESSAGE(FATAL_ERROR "build documentation selected, but sphinx-build could not be found")
ENDIF()

IF(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/user_doc/html
   AND IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/user_doc/man)
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
    CONFIGURE_FILE("${SPHINX_SRC_DIR}/conf.py" "${SPHINX_BUILD_DIR}/conf.py" @ONLY)
    ADD_CUSTOM_TARGET(doc ALL
                      DEPENDS sphinx-docs sphinx-manpages)

    # Group docs targets into a DocsTargets folder
    SET_PROPERTY(TARGET doc sphinx-docs sphinx-manpages
                 PROPERTY FOLDER cmake/DocTargets)

ELSEIF(HAVE_PREBUILT_DOCS)
    IF(NOT CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_CURRENT_BINARY_DIR)
        # Out of tree build - link the prebuilt documentation to the build tree
        ADD_CUSTOM_TARGET(link_doc ALL)
        ADD_CUSTOM_COMMAND(TARGET link_doc
            COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_CURRENT_SOURCE_DIR}/user_doc ${CMAKE_CURRENT_BINARY_DIR}/user_doc
            POST_BUILD)
    ENDIF()
ENDIF(BUILD_DOCS)
