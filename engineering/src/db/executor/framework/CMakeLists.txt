# engineering/src/db/executor/framework/CMakeLists.txt
add_library(executor_framework STATIC
    plan_to_exec.c
    executor_framework.c
)
target_link_libraries(executor_framework
    PUBLIC db_core db_vectorized
    PRIVATE executor_operators
)
