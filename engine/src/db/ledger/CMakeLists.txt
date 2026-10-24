# Ledger 模块
add_library(ledger_lib STATIC
    ledger.c
)
target_include_directories(ledger_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/include/db
    ${ENGINEERING_SOURCE_DIR}/include/db/core
)
