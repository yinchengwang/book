# 时空存储引擎模块

add_library(mm_st STATIC
    st_engine.c
)

target_include_directories(mm_st PUBLIC
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/include/db
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Task 14：st 白名单依赖 spatial（bbox/point 类型即来自 spatial_engine.h）
target_link_libraries(mm_st PUBLIC mm_base mm_spatial)
set_target_properties(mm_st PROPERTIES FOLDER "ModalityEngines")
target_compile_options(mm_st PRIVATE -Wno-unused-parameter)
