add_library(rag_fallback STATIC
    fallback.cpp
)
target_link_libraries(rag_fallback PUBLIC rag_core)
target_compile_features(rag_fallback PRIVATE cxx_std_17)