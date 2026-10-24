# RQ 量化器库

add_library(rq STATIC rq.c)

target_include_directories(rq PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${ENGINEERING_SOURCE_DIR}/include
)

target_link_libraries(rq PRIVATE project_includes Threads::Threads)