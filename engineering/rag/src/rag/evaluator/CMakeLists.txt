# RAG Evaluator 子模块
add_library(rag_evaluator OBJECT
    evaluator.cpp
)
target_include_directories(rag_evaluator PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
)
target_link_libraries(rag_evaluator PRIVATE
    rag_llm
    rag_logger
    nlohmann_json::nlohmann_json
)
