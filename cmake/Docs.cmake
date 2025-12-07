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

if(SPHINX_EXECUTABLE)
    option(BUILD_DOCS "build documentation (requires Sphinx)" ON)
else(SPHINX_EXECUTABLE)
    option(BUILD_DOCS "build documentation (requires Sphinx)" OFF)
endif(SPHINX_EXECUTABLE)

if(BUILD_DOCS AND NOT SPHINX_EXECUTABLE)
    message(FATAL_ERROR "build documentation selected, but sphinx-build could not be found")
endif()

if(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/user_doc/html
   AND IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/user_doc/man)
    set(HAVE_PREBUILT_DOCS TRUE)
else()
    set(HAVE_PREBUILT_DOCS FALSE)
endif()

if(BUILD_DOCS OR HAVE_PREBUILT_DOCS)
    set(INSTALL_DOCS ON)
else()
    set(INSTALL_DOCS OFF)
endif()

add_feature_info(Documentation INSTALL_DOCS "user manual and documentation")

set(USE_PREBUILT_DOCS FALSE)
if(BUILD_DOCS)
    configure_file("${SPHINX_SRC_DIR}/conf.py" "${SPHINX_BUILD_DIR}/conf.py" @ONLY)
    add_custom_target(doc ALL
                      DEPENDS sphinx-docs sphinx-manpages)

    # Group docs targets into a DocsTargets folder
    set_property(TARGET doc sphinx-docs sphinx-manpages
                 PROPERTY FOLDER cmake/DocTargets)

elseif(HAVE_PREBUILT_DOCS)
    set(USE_PREBUILT_DOCS TRUE)
    if(NOT CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_CURRENT_BINARY_DIR)
        # Out of tree build - link the prebuilt documentation to the build tree
        add_custom_target(link_doc ALL)
        add_custom_command(TARGET link_doc
            COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_CURRENT_SOURCE_DIR}/user_doc ${CMAKE_CURRENT_BINARY_DIR}/user_doc
            POST_BUILD)
    endif()
endif(BUILD_DOCS)
