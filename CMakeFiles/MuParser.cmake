INCLUDE(ExternalProject)

SET(MUPARSER_PREFIX muparser-build)
SET(MUPARSER_SRC ${CMAKE_CURRENT_SOURCE_DIR}/muparser-2.2.5)
SET(MUPARSER_OBJ ${MUPARSER_PREFIX}/obj)
SET(MUPARSER_DST ${MUPARSER_PREFIX}/dst)

# MuParser configure has an obnoxious victory message which we suppress.
EXTERNALPROJECT_ADD(
  muparser_project
  PREFIX ${MUPARSER_PREFIX}
  SOURCE_DIR ${MUPARSER_SRC}
  BINARY_DIR ${MUPARSER_OBJ}
  INSTALL_DIR ${MUPARSER_DST}
  CONFIGURE_COMMAND ${MUPARSER_SRC}/configure
                    --prefix=${CMAKE_CURRENT_BINARY_DIR}/${MUPARSER_DST}
                    --quiet --enable-shared=no --enable-samples=no --enable-debug=no > /dev/null
  BUILD_COMMAND make -j 3 CPPFLAGS=-D_UNICODE=1\ -Wno-switch lib/libmuparser.a
  INSTALL_COMMAND make install
  BUILD_BYPRODUCTS ${MUPARSER_DST}/lib/libmuparser.a
  EXCLUDE_FROM_ALL
)

ADD_LIBRARY(muparser STATIC IMPORTED)
SET_TARGET_PROPERTIES(muparser PROPERTIES IMPORTED_LOCATION
                      ${CMAKE_CURRENT_BINARY_DIR}/${MUPARSER_DST}/lib/libmuparser.a)
ADD_DEPENDENCIES(muparser muparser_project)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR}/${MUPARSER_DST}/include/)
