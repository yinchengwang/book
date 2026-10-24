# Multi-Probe LSH 库

add_library(lsh_multiprobe STATIC lsh_multiprobe.c)

target_include_directories(lsh_multiprobe PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${ENGINEERING_SOURCE_DIR}/include
)

target_link_libraries(lsh_multiprobe PRIVATE project_includes Threads::Threads)