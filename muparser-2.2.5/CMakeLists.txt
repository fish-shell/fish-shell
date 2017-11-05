SET(MUPARSER_SRCS
    ${CMAKE_CURRENT_LIST_DIR}/src/muParser.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/muParserBase.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/muParserBytecode.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/muParserCallback.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/muParserDLL.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/muParserError.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/muParserInt.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/muParserTokenReader.cpp)

ADD_LIBRARY(muparser STATIC ${MUPARSER_SRCS})
TARGET_INCLUDE_DIRECTORIES(muparser PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)
