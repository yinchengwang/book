# 文档聚合管道测试
# engineering/test/db/storage/doc/CMakeLists.txt

# 文档聚合管道测试
add_executable(test_doc_pipeline
    doc_pipeline_test.cpp
)

# 包含源码（因为 doc 模块未启用，需要直接编译）
target_sources(test_doc_pipeline PRIVATE
    ${ENGINEERING_SOURCE_DIR}/src/db/storage/doc/doc_pipeline.c
)

target_link_libraries(test_doc_pipeline
    PRIVATE
    db_core
    project_includes
    gtest
    gtest_main
)

# 包含必要目录
target_include_directories(test_doc_pipeline PRIVATE
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/include/db
    ${ENGINEERING_SOURCE_DIR}/include/db/storage
    ${ENGINEERING_SOURCE_DIR}/src/db/storage/doc
    ${CMAKE_SOURCE_DIR}/third_part/googletest/googletest/include
    ${CMAKE_SOURCE_DIR}/third_part/googletest/googlemock/include
)

# Windows MSVC 运行时库
if(MSVC)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "")
        target_compile_options(test_doc_pipeline PRIVATE /W4 /MDd)
    else()
        target_compile_options(test_doc_pipeline PRIVATE /W4 /MD)
    endif()
else()
    target_compile_options(test_doc_pipeline PRIVATE -Wall -Wextra)
endif()

# 输出到构建目录
set_target_properties(test_doc_pipeline PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# 注册测试
add_test(NAME test_doc_pipeline COMMAND test_doc_pipeline)
