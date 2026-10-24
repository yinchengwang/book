# add_subdirectory(bst)
add_subdirectory(trie)

# 递归查找当前文件夹下的子文件夹中的.cpp文件
file(GLOB_RECURSE SELF_MADE_CPP_SOURCE_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.cpp")

add_library(self_made_cpp STATIC ${SELF_MADE_CPP_SOURCE_FILES})

target_link_libraries(self_made_cpp
    PRIVATE
        project_includes
)

set_target_properties(self_made_cpp PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    FOLDER "Libraries"
)

message(STATUS "SELF_MADE_CPP_SOURCE_FILES: ${SELF_MADE_CPP_SOURCE_FILES}")
