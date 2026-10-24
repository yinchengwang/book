# 分布式死锁检测模块
add_library(db_deadlock STATIC
    deadlock.c
)

target_include_directories(db_deadlock PUBLIC
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/include/db/distributed
)

target_link_libraries(db_deadlock PUBLIC
    db_core
)

target_compile_options(db_deadlock PRIVATE -Wno-unused-parameter)

set_target_properties(db_deadlock PROPERTIES FOLDER "Libraries")