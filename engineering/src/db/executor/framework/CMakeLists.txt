# engineering/src/db/executor/framework/CMakeLists.txt
add_library(executor_framework STATIC
    plan_to_exec.c
    # executor_framework.c  # Task 3 创建
)
target_link_libraries(executor_framework
    PUBLIC db_core db_vectorized
)
