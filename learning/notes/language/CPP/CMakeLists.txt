# learning/notes/language/CPP/CMakeLists.txt
# C++ 学习笔记构建配置
# 每个 .cpp 文件生成一个独立的可执行文件

cmake_minimum_required(VERSION 3.20)

project(CPP_Notes
    VERSION 1.0.0
    DESCRIPTION "C++ learning notes"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 辅助函数：为指定子目录下的所有 .cpp 创建可执行文件
function(add_cpp_examples SUBDIR OUTPUT_DIR)
    set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/${SUBDIR}")
    set(BIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/99_build/${OUTPUT_DIR}")

    if(NOT EXISTS "${SRC_DIR}")
        message(WARNING "目录不存在: ${SRC_DIR}")
        return()
    endif()

    file(GLOB CPP_FILES "${SRC_DIR}/*.cpp")

    foreach(CPP_FILE ${CPP_FILES})
        # 获取文件名（不含扩展名）
        get_filename_component(NAME ${CPP_FILE} NAME_WE)

        # 读取文件内容，检查是否为空文件
        file(READ ${CPP_FILE} FILE_CONTENT)
        string(STRIP "${FILE_CONTENT}" STRIPPED_CONTENT)

        if("${STRIPPED_CONTENT}" STREQUAL "")
            message(STATUS "跳过空文件: ${CPP_FILE}")
            continue()
        endif()

        # 创建可执行文件
        add_executable(${NAME} ${CPP_FILE})

        # 设置输出目录
        set_target_properties(${NAME} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${BIN_DIR}"
        )

        message(STATUS "已注册: ${NAME} -> ${BIN_DIR}/${NAME}.exe")
    endforeach()
endfunction()

# 构建 01_base 目录下的示例
add_cpp_examples("01_base" "01_baseTest")

# 构建 02_header 目录下的示例（目录为空时自动跳过）
add_cpp_examples("02_header" "02_headerTest")