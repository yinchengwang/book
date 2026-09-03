add_library(rag_incremental STATIC
    incremental_indexer.cpp
)
target_link_libraries(rag_incremental PUBLIC rag_core)
target_compile_features(rag_incremental PRIVATE cxx_std_17)