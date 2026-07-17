# RAG LLM 子模块
find_package(CURL QUIET)
if(CURL_FOUND)
    message(STATUS "CURL: found")
    set(RAG_CURL_LIBS CURL::libcurl)
    add_definitions(-DRAG_USE_CURL)
else()
    message(STATUS "CURL: not found, will use stub HTTP")
    set(RAG_CURL_LIBS "")
endif()

add_library(rag_llm OBJECT
    llm_service.cpp
    ollama_embedding.cpp
    ollama_llm.cpp
)
target_include_directories(rag_llm PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
)
target_link_libraries(rag_llm PRIVATE
    ${RAG_CURL_LIBS}
    nlohmann_json::nlohmann_json
)
