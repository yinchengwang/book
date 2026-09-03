add_library(rag_llm STATIC
    llm_client.cpp
)
target_link_libraries(rag_llm PUBLIC rag_core)
target_compile_features(rag_llm PRIVATE cxx_std_17)

# 链接 curl
find_package(CURL REQUIRED)
target_link_libraries(rag_llm PRIVATE CURL::libcurl)