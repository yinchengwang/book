# Modular LLM 模块 CMakeLists.txt
# LlamaService 基于 llama.cpp 实现

# 头文件目录
set(MODULAR_LLM_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/rag/modular/llm
    ${CMAKE_SOURCE_DIR}/third_party/llama.cpp
    ${CMAKE_SOURCE_DIR}/third_party/llama.cpp/common
)

# 源文件
set(MODULAR_LLM_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rag/modular/llm/llama_service.cpp
)

# 创建静态库
add_library(rag_modular_llm STATIC
    ${MODULAR_LLM_SOURCES}
)

# 设置包含目录
target_include_directories(rag_modular_llm PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_include_directories(rag_modular_llm PRIVATE
    ${MODULAR_LLM_INCLUDE_DIRS}
)

# 链接 llama.cpp 静态库
if(EXISTS ${CMAKE_SOURCE_DIR}/third_party/llama.cpp/CMakeLists.txt)
    # llama.cpp 已作为子项目编译
    target_link_libraries(rag_modular_llm PRIVATE
        llama
        common
    )
endif()

# C++ 标准
target_compile_features(rag_modular_llm PRIVATE
    cxx_std_17
)

# 导出
add_library(rag::modular_llm ALIAS rag_modular_llm)

message(STATUS "Modular LLM module configured")
