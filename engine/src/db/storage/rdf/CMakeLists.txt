# RDF 知识图谱引擎模块

add_library(storage_rdf OBJECT
    rdf_engine.c
    rdf_index.c
    sparql_parser.c
)

target_include_directories(storage_rdf PUBLIC
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/include/db
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_options(storage_rdf PRIVATE -Wno-unused-parameter)
