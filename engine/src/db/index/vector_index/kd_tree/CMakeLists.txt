# KD-Tree 索引

add_library(kd_tree STATIC
    kd_tree.c
)

target_include_directories(kd_tree PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${ENGINEERING_SOURCE_DIR}/include
)

target_link_libraries(kd_tree PRIVATE
    project_includes
    Threads::Threads
)