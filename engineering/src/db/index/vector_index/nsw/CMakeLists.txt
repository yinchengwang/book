# NSW 索引库 CMakeLists.txt

add_project_library(nsw NSW_SRCS)

target_link_libraries(nsw
    PUBLIC
        project_includes
        algo-prod
        heap_vector_store
)