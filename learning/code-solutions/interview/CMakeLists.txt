# 面试题解集合（学习层）
# 来源：阿里/百度/华为/京东/美团等公司面试题
# 用 GLOB_RECURSE 自动收集所有 .c/.cpp 文件，构建 interview-solutions 静态库

file(GLOB_RECURSE INTERVIEW_SOURCES CONFIGURE_DEPENDS
    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    "*.c" "*.cpp"
)

# 排除 CMakeLists.txt 自身（如果有）
list(FILTER INTERVIEW_SOURCES EXCLUDE REGEX "CMakeLists\.txt$")

if(INTERVIEW_SOURCES)
    add_library(interview-solutions STATIC ${INTERVIEW_SOURCES})

    target_include_directories(interview-solutions PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
    )

    target_link_libraries(interview-solutions PUBLIC learning_includes)

    set_target_properties(interview-solutions PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
        FOLDER "Libraries"
    )

    message(STATUS "INTERVIEW_SOURCES count: ${INTERVIEW_SOURCES}")
endif()