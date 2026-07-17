# 工程层 Raft 共识模块
add_library(db_consensus STATIC raft.c raft_cluster.c)

target_include_directories(db_consensus PUBLIC
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/include/db/consensus
)

target_link_libraries(db_consensus PUBLIC db_core)
target_compile_options(db_consensus PRIVATE -Wno-unused-parameter)

set_target_properties(db_consensus PROPERTIES FOLDER "Libraries")
