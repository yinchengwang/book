# Annoy 索引

add_library(annoy STATIC
    annoy.c
)

target_include_directories(annoy PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${ENGINEERING_SOURCE_DIR}/include
)

target_link_libraries(annoy PRIVATE
    project_includes
    Threads::Threads
)
