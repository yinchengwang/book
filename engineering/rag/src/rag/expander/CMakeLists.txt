add_library(rag_expander STATIC
    query_expander.cpp
    hyde_expander.cpp
    synonym_expander.cpp
)
target_link_libraries(rag_expander PUBLIC rag_core)

# 设置 C++ 标准
target_compile_features(rag_expander PRIVATE cxx_std_17)