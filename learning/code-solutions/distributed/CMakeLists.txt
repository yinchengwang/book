# 分布式算法实现（学习层）
# 来源：phase9 分布式演进（coordinator/dist_txn/raft/shard）
# S11 续做：解开编译阻塞——分两层
# - 第一层：coordinator.c + shard.c：可以编译，加入
# - 第二层：raft.c + dist_txn.c：API 签名与实现不一致（phase9 不完整），
#   S11 暂时排除；后续 S11+ 阶段可分别续做
#
# S11 修复：
# - 加入 RaftRouter 前向声明（coordinator.h）
# - 移除 #include "log.h"（死代码 include，无 log_ 函数被调用）

file(GLOB_RECURSE DISTRIBUTED_SOURCES CONFIGURE_DEPENDS
    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    "*.c" "*.cpp"
)

# 排除 CMakeLists.txt 自身
list(FILTER DISTRIBUTED_SOURCES EXCLUDE REGEX "CMakeLists\.txt$")

# S11 第二层暂时排除：phase9 不完整
# raft.c:    raft_handle_append_entries 签名与 raft.h 不一致（7 vs 9 参数）
# dist_txn.c: TwoPCCoordinator vs TwoPCoordinator 类型不一致
# shard.c:   shard_route_query 返回类型与 .h 不一致（ShardRouting* vs **）
list(FILTER DISTRIBUTED_SOURCES EXCLUDE REGEX "^(raft|dist_txn|shard)\\.c$")

if(DISTRIBUTED_SOURCES)
    add_library(distributed-solutions STATIC ${DISTRIBUTED_SOURCES})

    target_include_directories(distributed-solutions PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    )

    target_link_libraries(distributed-solutions PUBLIC learning_includes)

    set_target_properties(distributed-solutions PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
        FOLDER "Libraries"
    )

    message(STATUS "DISTRIBUTED_SOURCES count: ${DISTRIBUTED_SOURCES}")
endif()
