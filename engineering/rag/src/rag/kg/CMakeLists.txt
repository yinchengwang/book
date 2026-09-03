add_library(rag_kg STATIC
    kg_enhancer.cpp
)
target_link_libraries(rag_kg PUBLIC rag_core)
target_compile_features(rag_kg PRIVATE cxx_std_17)
