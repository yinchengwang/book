# PQ 量化器

file(GLOB PQ_SOURCES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c")

add_library(pq STATIC ${PQ_SOURCES})

target_include_directories(pq PUBLIC
    ${ENGINEERING_SOURCE_DIR}/include
)

target_link_libraries(pq PUBLIC
    project_includes
)