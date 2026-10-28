# storage/mm 模块：多模态存储管理
#
# Task 6 分层拆分：
#   storage_mm      —— mm_pool.c（对象池），留在 db_storage 聚合内。
#   mm_storage_api  —— L1 公共层：engine_registry.c（自 core 迁入）+
#                      mm_storage.c（自 storage_mm 迁出）+ 构建期生成的
#                      注册收集器（由 src/db/CMakeLists.txt 注入）。
#                      只依赖 mm_base；引擎符号引用由链接闭包提供。
file(GLOB_RECURSE MM_SOURCE_FILES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c")

# mm_storage.c 迁入 mm_storage_api，不再编入 storage_mm
list(FILTER MM_SOURCE_FILES EXCLUDE REGEX "mm_storage\\.c$")

if(MM_SOURCE_FILES)
    add_library(storage_mm OBJECT ${MM_SOURCE_FILES})
    target_include_directories(storage_mm PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${ENGINEERING_SOURCE_DIR}/include
        ${ENGINEERING_SOURCE_DIR}/include/db
        ${ENGINEERING_SOURCE_DIR}/include/db/storage
        ${ENGINEERING_SOURCE_DIR}/include/db/core
    )
    target_link_libraries(storage_mm PRIVATE multimodal_config_gen)
endif()

# L1 公共层：引擎注册表 + 多模态存储路由
add_library(mm_storage_api STATIC
    ${ENGINEERING_SOURCE_DIR}/src/db/core/engine_registry.c
    ${CMAKE_CURRENT_SOURCE_DIR}/mm_storage.c
)
target_include_directories(mm_storage_api PUBLIC
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/include/db
    ${ENGINEERING_SOURCE_DIR}/include/db/storage
)
target_link_libraries(mm_storage_api PUBLIC mm_base)
set_target_properties(mm_storage_api PROPERTIES FOLDER "Libraries")
