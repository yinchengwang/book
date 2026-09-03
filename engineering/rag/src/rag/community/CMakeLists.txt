add_library(rag_community STATIC
    community.cpp
)
target_link_libraries(rag_community PUBLIC rag_core)
target_compile_features(rag_community PRIVATE cxx_std_17)