# RDF 知识图谱引擎模块

add_library(mm_rdf STATIC
    rdf_engine.c
    rdf_index.c
    sparql_parser.c
)

target_include_directories(mm_rdf PUBLIC
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/include/db
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(mm_rdf PUBLIC mm_base)
set_target_properties(mm_rdf PROPERTIES FOLDER "ModalityEngines")
target_compile_options(mm_rdf PRIVATE -Wno-unused-parameter)
