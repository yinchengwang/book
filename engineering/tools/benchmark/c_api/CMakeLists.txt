cmake_minimum_required(VERSION 3.20)
project(vector_index_c_api C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ========== 路径定义 ==========
# tools/benchmark/c_api/ 上溯 3 层到达工程根 book/engineering/
get_filename_component(ENGINEERING_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
get_filename_component(BUILD_DIR "${ENGINEERING_ROOT}/build/engineering" ABSOLUTE)

# ========== 包含路径 ==========
include_directories(
    ${ENGINEERING_ROOT}/include
    ${ENGINEERING_ROOT}/include/db/index/vector_index
    ${ENGINEERING_ROOT}/include/db/index/vector_index/hnsw
    ${ENGINEERING_ROOT}/include/db/index/vector_index/delete
    ${ENGINEERING_ROOT}/src/db/index/vector_index
    ${ENGINEERING_ROOT}/src/db/index/vector_index/hnsw
    ${ENGINEERING_ROOT}/src/algo-prod
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# ========== 查找已构建的静态库 ==========
# 从 CMake 当前二进制目录推断（实际构建在 c:/code/book/build/engineering）
set(LIB_SEARCH_PATHS
    ${CMAKE_BINARY_DIR}/lib
    ${CMAKE_BINARY_DIR}/src/db/index/vector_index
    ${CMAKE_BINARY_DIR}/src/db/index/vector_index/faiss_hnsw
    ${CMAKE_BINARY_DIR}
)

find_library(VECTOR_INDEX_LIB NAMES vector_index PATHS ${LIB_SEARCH_PATHS} NO_DEFAULT_PATH)
find_library(ALGO_PROD_LIB NAMES algo-prod PATHS ${LIB_SEARCH_PATHS} NO_DEFAULT_PATH)
find_library(FAISS_HNSW_LIB NAMES faiss_hnsw PATHS ${LIB_SEARCH_PATHS} NO_DEFAULT_PATH)

if(NOT VECTOR_INDEX_LIB)
    message(WARNING "vector_index 静态库未找到，HNSW API 将不可用")
endif()

# ========== 构建 DLL ==========
add_library(vector_index_c_api SHARED vector_index_c_api.c)

if(VECTOR_INDEX_LIB)
    target_link_libraries(vector_index_c_api PRIVATE ${VECTOR_INDEX_LIB})
endif()
if(ALGO_PROD_LIB)
    target_link_libraries(vector_index_c_api PRIVATE ${ALGO_PROD_LIB})
endif()
if(FAISS_HNSW_LIB)
    target_link_libraries(vector_index_c_api PRIVATE ${FAISS_HNSW_LIB})
endif()

if(WIN32)
    target_compile_definitions(vector_index_c_api PRIVATE VECTOR_INDEX_EXPORTS)
endif()

# ========== 输出到 build/engineering（与 ctypes 查找路径一致） ==========
set_target_properties(vector_index_c_api PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${BUILD_DIR}
    RUNTIME_OUTPUT_DIRECTORY ${BUILD_DIR}
    PREFIX ""
)
