# Aging 模块
add_library(aging_lib STATIC
    aging.c
    aging_scheduler.c
)
target_include_directories(aging_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/include/db
    ${ENGINEERING_SOURCE_DIR}/include/db/core
)
target_link_libraries(aging_lib PRIVATE pthread)
