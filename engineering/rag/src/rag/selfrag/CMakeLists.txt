add_library(rag_selfrag STATIC
    self_rag.cpp
)
target_link_libraries(rag_selfrag PUBLIC rag_core)
target_compile_features(rag_selfrag PRIVATE cxx_std_17)