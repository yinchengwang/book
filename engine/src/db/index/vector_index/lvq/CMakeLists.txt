# LVQ 量化器库

add_library(lvq STATIC lvq.c)

target_include_directories(lvq PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${ENGINEERING_SOURCE_DIR}/include
)

target_link_libraries(lvq PRIVATE project_includes Threads::Threads)