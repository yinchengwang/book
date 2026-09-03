# SSG 索引库 CMakeLists.txt

add_project_library(ssg SSG_SRCS)

target_link_libraries(ssg
    PUBLIC
        project_includes
        algo-prod
        heap_vector_store
)