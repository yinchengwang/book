# ITQ 量化器库

add_library(itq STATIC itq.c)

target_include_directories(itq PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${ENGINEERING_SOURCE_DIR}/include
)

target_link_libraries(itq PRIVATE project_includes Threads::Threads)