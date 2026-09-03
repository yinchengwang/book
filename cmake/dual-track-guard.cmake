# dual-track-guard.cmake
# 跨轨引用守卫：检测并拒绝 learning 与 engineering 轨道的相互引用

# 使用方法（必须在每个轨道的 CMakeLists.txt 中调用）：
#   include("${CMAKE_CURRENT_LIST_DIR}/../cmake/dual-track-guard.cmake")
#   guard_no_cross_track_reference("learning/")  # 或 "engineering/"

function(guard_no_cross_track_reference forbidden_pattern)
    # 跳过根目录 dispatcher（它必须 add_subdirectory 两个轨道）
    get_filename_component(_current_dir "${CMAKE_CURRENT_LIST_DIR}" NAME)

    if(_current_dir STREQUAL "learning" OR _current_dir STREQUAL "engineering")
        file(READ "${CMAKE_CURRENT_LIST_FILE}" _content)

        # 移除所有注释行（以 # 开头，可有前导空白）
        string(REGEX REPLACE "^[ \t]*#[^\n]*\n" "" _content "${_content}")
        string(REGEX REPLACE "\n[ \t]*#[^\n]*" "" _content "${_content}")

        # 移除 guard 函数调用自身的字符串参数（避免误报）
        string(REPLACE "guard_no_cross_track_reference(\"${forbidden_pattern}\")" "" _content "${_content}")
        string(REPLACE "guard_no_cross_track_reference(${forbidden_pattern})" "" _content "${_content}")

        # 只匹配函数调用 add_subdirectory/target_link_libraries/target_include_directories
        string(REGEX MATCHALL
            "(add_subdirectory|target_link_libraries|target_include_directories)[^)]*${forbidden_pattern}"
            _matches
            "${_content}"
        )
        list(LENGTH _matches _match_count)
        if(_match_count GREATER 0)
            message(FATAL_ERROR
                "[dual-track-guard] ${CMAKE_CURRENT_LIST_FILE} references forbidden path '${forbidden_pattern}' "
                "in add_subdirectory/target_link_libraries/target_include_directories. "
                "Tracks must not reference each other directly. See openspec/changes/dual-track-top-level/spec.md.")
        endif()
    endif()
endfunction()