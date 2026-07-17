# SQ 量化器

file(GLOB SQ_SOURCES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c")

add_library(sq STATIC ${SQ_SOURCES})

target_include_directories(sq PUBLIC
    ${ENGINEERING_SOURCE_DIR}/include
)

target_link_libraries(sq PUBLIC
    project_includes
)