# engineering/src/db/executor/operators/CMakeLists.txt
add_library(executor_operators STATIC
    seqscan_exec.c
)
target_link_libraries(executor_operators
    PUBLIC executor_framework
    PRIVATE db_vectorized
)