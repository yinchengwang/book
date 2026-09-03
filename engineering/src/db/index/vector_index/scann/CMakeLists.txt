# ScaNN 库

add_library(scann STATIC scann.c)

target_include_directories(scann PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${ENGINEERING_SOURCE_DIR}/include
)

target_link_libraries(scann PRIVATE project_includes Threads::Threads)