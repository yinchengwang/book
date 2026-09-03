# Subscription 模块
add_library(subscription_lib STATIC
    cdc.c
    cdc_wal.c
    subscription.c
)
target_include_directories(subscription_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/include/db
    ${ENGINEERING_SOURCE_DIR}/include/db/core
)
