file(GLOB GAME2048_SOURCES CONFIGURE_DEPENDS "*.c")
add_executable(game2048 ${GAME2048_SOURCES})

target_link_libraries(game2048 PRIVATE project_includes)

set_target_properties(game2048 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)
