find_program(SPHINX_EXECUTABLE NAMES sphinx-build
    HINTS
    $ENV{SPHINX_DIR}
    PATH_SUFFIXES bin
    DOC "Sphinx documentation generator")

include(FeatureSummary)

set(SPHINX_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/doc_src")
set(SPHINX_ROOT_DIR "${CMAKE_CURRENT_BINARY_DIR}/user_doc")
set(SPHINX_BUILD_DIR "${SPHINX_ROOT_DIR}/build")
set(SPHINX_HTML_DIR "${SPHINX_ROOT_DIR}/html")
set(SPHINX_MANPAGE_DIR "${SPHINX_ROOT_DIR}/man")

# sphinx-docs uses fish_indent for highlighting.
# Prepend the output dir of fish_indent to PATH.
add_custom_target(sphinx-docs
    mkdir -p ${SPHINX_HTML_DIR}/_static/
    COMMAND env PATH="${CMAKE_BINARY_DIR}:$$PATH"
        ${SPHINX_EXECUTABLE}
        -j auto
        -q -b html
        -c "${SPHINX_SRC_DIR}"
        -d "${SPHINX_ROOT_DIR}/.doctrees-html"
        "${SPHINX_SRC_DIR}"
        "${SPHINX_HTML_DIR}"
    DEPENDS ${SPHINX_SRC_DIR}/fish_indent_lexer.py fish_indent
    COMMENT "Building HTML documentation with Sphinx")

add_custom_target(sphinx-manpages
    env FISH_BUILD_VERSION_FILE=${CMAKE_CURRENT_BINARY_DIR}/${FBVF}
        ${SPHINX_EXECUTABLE}
        -j auto
        -q -b man
        -c "${SPHINX_SRC_DIR}"
        -d "${SPHINX_ROOT_DIR}/.doctrees-man"
        "${SPHINX_SRC_DIR}"
        "${SPHINX_MANPAGE_DIR}/man1"
    DEPENDS CHECK-FISH-BUILD-VERSION-FILE
    COMMENT "Building man pages with Sphinx")

if(NOT DEFINED WITH_DOCS) # Don't check for legacy options if the new one is defined, to help bisecting.
    if(DEFINED BUILD_DOCS)
        message(FATAL_ERROR "the BUILD_DOCS option is no longer supported, use -DWITH_DOCS=ON|OFF")
    endif()
    if(DEFINED INSTALL_DOCS)
        message(FATAL_ERROR "the INSTALL_DOCS option is no longer supported, use -DWITH_DOCS=ON|OFF")
    endif()
endif()

if(SPHINX_EXECUTABLE)
    option(WITH_DOCS "build documentation (requires Sphinx)" ON)
else()
    option(WITH_DOCS "build documentation (requires Sphinx)" OFF)
endif()

if(WITH_DOCS AND NOT SPHINX_EXECUTABLE)
    message(FATAL_ERROR "build documentation selected, but sphinx-build could not be found")
endif()

add_feature_info(Documentation WITH_DOCS "user manual and documentation")

if(WITH_DOCS)
    configure_file("${SPHINX_SRC_DIR}/conf.py" "${SPHINX_BUILD_DIR}/conf.py" @ONLY)
    add_custom_target(doc ALL
                      DEPENDS sphinx-docs sphinx-manpages)
    # Group docs targets into a DocsTargets folder
    set_property(TARGET doc sphinx-docs sphinx-manpages
                 PROPERTY FOLDER cmake/DocTargets)
endif()
