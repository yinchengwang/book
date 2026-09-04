# Modular RAG 核心模块 CMakeLists.txt

# 头文件目录
set(MODULAR_CORE_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/rag/modular
)

# 源文件
set(MODULAR_CORE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rag/modular/types.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rag/modular/agent/tool.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rag/modular/pipeline/pipeline_base.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rag/modular/pipeline/pipeline_factory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rag/modular/pipeline/naive_pipeline.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rag/modular/pipeline/advanced_pipeline.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rag/modular/pipeline/hybrid_pipeline.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rag/modular/pipeline/hyde_pipeline.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rag/modular/pipeline/graph_pipeline.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rag/modular/pipeline/corrective_pipeline.cpp
)

# 创建静态库
add_library(rag_modular_core STATIC
    ${MODULAR_CORE_SOURCES}
)

# 设置包含目录
target_include_directories(rag_modular_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_include_directories(rag_modular_core PRIVATE
    ${MODULAR_CORE_INCLUDE_DIRS}
)

# C++ 标准
target_compile_features(rag_modular_core PRIVATE
    cxx_std_17
)

# 导出
add_library(rag::modular_core ALIAS rag_modular_core)

add_subdirectory(src/rag/modular/llm)
add_subdirectory(src/rag/modular/api)
add_subdirectory(src/rag/modular/cli)
add_subdirectory(src/rag/modular/agent)
add_subdirectory(src/rag/modular/pipeline)

message(STATUS "Modular modules configured")
