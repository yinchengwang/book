add_library(rag_benchmark STATIC
    benchmark.cpp
)
target_link_libraries(rag_benchmark PUBLIC rag_core)
target_compile_features(rag_benchmark PRIVATE cxx_std_17)