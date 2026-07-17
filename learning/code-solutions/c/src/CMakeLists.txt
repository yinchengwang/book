add_subdirectory(c)
add_subdirectory(cpp)

# 递归查找当前文件夹下的子文件夹中的.c文件
file(GLOB_RECURSE LEETCODE_SOURCE_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "c/*.c" "cpp/*.cpp")

add_library(leetcode STATIC ${LEETCODE_SOURCE_FILES})

target_link_libraries(leetcode
    PRIVATE
        project_includes
)

set_target_properties(leetcode PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    FOLDER "Libraries"
)

message(STATUS "LEETCODE_SOURCE_FILES: ${LEETCODE_SOURCE_FILES}")
