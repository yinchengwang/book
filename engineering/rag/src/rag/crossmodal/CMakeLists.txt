# RAG Crossmodal Retrieval 子模块
add_library(rag_crossmodal STATIC
    crossmodal_retriever.cpp
)
target_link_libraries(rag_crossmodal PUBLIC rag_core rag_retrieval rag_embedding)
target_compile_features(rag_crossmodal PRIVATE cxx_std_17)
target_include_directories(rag_crossmodal PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
)