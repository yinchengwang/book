add_library(rag_eval STATIC
    evaluator.cpp
)
target_link_libraries(rag_eval PUBLIC rag_core)
target_compile_features(rag_eval PRIVATE cxx_std_17)