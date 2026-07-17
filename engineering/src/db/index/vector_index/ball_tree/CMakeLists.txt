# Ball-Tree 库

add_library(ball_tree STATIC ball_tree.c)

target_include_directories(ball_tree PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${ENGINEERING_SOURCE_DIR}/include
)

target_link_libraries(ball_tree PRIVATE project_includes Threads::Threads)