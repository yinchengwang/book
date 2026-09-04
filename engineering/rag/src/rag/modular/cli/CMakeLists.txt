# Modular RAG CLI 子模块
add_executable(modular_rag
    main.cpp
)
target_include_directories(modular_rag PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
)
target_link_libraries(modular_rag PRIVATE
    rag_modular_core
    rag_modular_api
    rag_logger
    Threads::Threads
    ws2_32
)
