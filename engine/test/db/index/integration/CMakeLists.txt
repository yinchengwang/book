# 集成测试 CMake 配置
#
# 不使用 add_project_test 宏：该宏用 file(GLOB *.cpp *.c) 收集源文件，
# 无法排除下述两个文件，而它们不能参与 integration_test 的编译：
#
#   diskann_recall_simple.cpp   —— 自带 main()，是独立运行的召回率程序，
#                                  与 integration_test.cpp 的 main() 冲突
#                                  （链接期 multiple definition of `main'）
#   diskann_extended_stubs.c    —— 扩展 API 的桩实现，与 libdiskann.a 内的
#                                  diskann_stubs.c 重复定义同一批符号
#                                  （diskann_spann_create / diskann_fresh_* /
#                                    diskann_merge_context_create 等 8 个），
#                                  链接期报 multiple definition
#                                  已用 nm 核实：库级桩完全覆盖测试级桩，
#                                  且多出 5 个符号，故测试级桩纯属冗余。
#
#   multi_index_joint_test.cpp  —— 依赖 faiss_hnsw_index_set_heap_store()，
#                                  该接口只在 include/db/index/vector_index/hnsw/
#                                  faiss_hnsw.h 中声明（Heap 向量存储作为
#                                  single source of truth 的设计），但实际编译的
#                                  结构体定义在 faiss_hnsw/faiss_hnsw_internal.h，
#                                  **没有 heap_store 字段**，实现也从未落地。
#                                  链接期报 undefined reference。
#                                  这是未实现的功能，不是构建配置问题；补一个
#                                  只存指针的假实现会让测试断言通过但语义为假，
#                                  故此处排除，待 Heap 存储真正实现后再启用。
#
# diskann_recall_simple.cpp 另建独立 target，保留其测试价值。

file(GLOB INTEGRATION_TEST_SRCS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
)
list(FILTER INTEGRATION_TEST_SRCS EXCLUDE REGEX ".*diskann_recall_simple\\.cpp$")
list(FILTER INTEGRATION_TEST_SRCS EXCLUDE REGEX ".*diskann_extended_stubs\\.c$")
list(FILTER INTEGRATION_TEST_SRCS EXCLUDE REGEX ".*multi_index_joint_test\\.cpp$")

add_executable(integration_test ${INTEGRATION_TEST_SRCS})

target_link_libraries(integration_test
    PRIVATE
        project_includes
        GTest::gtest_main
        mm_vector_index
        diskann
        ivf
        heap_vector_store
        storage_backend_common
        algo-prod
)

# db_utils 的源文件可能为空（utils/ 目前只有 CMakeLists.txt），
# 此时 add_library 不会执行、目标不存在，直接链接会导致配置期报错。
if(TARGET db_utils)
    target_link_libraries(integration_test PRIVATE db_utils)
endif()

set_target_properties(integration_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    FOLDER "Tests"
)

include(GoogleTest)
gtest_discover_tests(integration_test
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES LABELS "integration_test"
)

# DiskANN 召回率独立程序（自带 main，不属于 gtest 测试）
add_executable(diskann_recall_simple diskann_recall_simple.cpp)
target_link_libraries(diskann_recall_simple
    PRIVATE
        project_includes
        diskann
        algo-prod
)
set_target_properties(diskann_recall_simple PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    FOLDER "Tests"
)
