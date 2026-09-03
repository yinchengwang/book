add_library(rag_multimodal STATIC
    multimodal_parser.cpp
)
target_link_libraries(rag_multimodal PUBLIC rag_core)
target_compile_features(rag_multimodal PRIVATE cxx_std_17)