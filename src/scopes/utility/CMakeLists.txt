add_subdirectory(internal)

set(SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/BufferedResultForwarder.cpp
)

set(UNITY_SCOPES_LIB_SRC ${UNITY_SCOPES_LIB_SRC} ${SRC} PARENT_SCOPE)
