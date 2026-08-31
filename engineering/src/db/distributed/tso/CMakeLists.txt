# 分布式 TSO 模块
add_library(db_tso STATIC
    tso_codec.c
    tso_client.c
)

target_include_directories(db_tso PUBLIC
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/include/db/distributed
)

target_link_libraries(db_tso PUBLIC
    db_core
)

target_compile_options(db_tso PRIVATE -Wno-unused-parameter)

set_target_properties(db_tso PROPERTIES FOLDER "Libraries")