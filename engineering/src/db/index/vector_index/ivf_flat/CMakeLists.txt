# IVF-Flat 索引

add_library(ivf_flat STATIC
    ivf_flat.c
)

target_include_directories(ivf_flat PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${ENGINEERING_SOURCE_DIR}/include
)

target_link_libraries(ivf_flat PRIVATE
    project_includes
    algo-prod
    Threads::Threads
)