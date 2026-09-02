# engineering/src/db/executor/operators/CMakeLists.txt
add_library(executor_operators STATIC
    seqscan_exec.c
    filter_exec.c
    project_exec.c
    hashjoin_exec.c
    hashagg_exec.c
)
target_link_libraries(executor_operators
    PUBLIC executor_framework
    PRIVATE db_vectorized
)
