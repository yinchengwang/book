# ========================================================================
# mm_smoke.cmake —— 模态独立冒烟目标生成器
#
# mm_smoke(<modality>) 生成可执行目标 mm_<modality>_smoke：
#   只链接 mm_base + mm_storage_api + mm_<modality>。
#   不得链接 db_storage 或任何其他模态库——链不上即隔离失败，
#   这正是冒烟目标存在的意义（链接期机械证明模态独立可用）。
#
# 冒烟断言：注册表中的非空槽位集合 == 本 profile 的期望引擎掩码
# （见 cmake/mm_smoke_expect.h.in，由 MMDB_EXPECTED_ENGINE_MASK 生成）。
# 单模态 profile 下掩码只有本模态一位，等价于「只注册本模态引擎」；
# legacy/full profile 下掩码覆盖全部启用模态，断言同样成立。
#
# 注意：relational 模态不实现 storage_ops_t，其冒烟在 Task 15 单独处理。
# ========================================================================

# 生成 mm_smoke_expect.h（每个构建树一次）
set(_expect_dir "${CMAKE_BINARY_DIR}/generated/include/db")
file(MAKE_DIRECTORY "${_expect_dir}")
configure_file(
    "${CMAKE_CURRENT_LIST_DIR}/mm_smoke_expect.h.in"
    "${_expect_dir}/mm_smoke_expect.h"
    @ONLY
)
set(MMDB_SMOKE_EXPECT_HEADER "${_expect_dir}/mm_smoke_expect.h"
    CACHE INTERNAL "generated mm_smoke_expect.h path")

# mm_smoke(<modality> [LIB <target>])
#   <modality> 对应 profile 模态名；LIB 缺省为 mm_<modality>，
#   但库名与模态名不一一对应时（tree -> mm_yang）需显式指定。
function(mm_smoke modality)
    cmake_parse_arguments(MMS "" "LIB" "" ${ARGN})
    set(_lib "${MMS_LIB}")
    if(NOT _lib)
        set(_lib "mm_${modality}")
    endif()
    set(_exe mm_${modality}_smoke)
    # 源文件跟随库名（tree -> mm_yang_smoke.c），exe 名跟随 profile 模态名
    set(_src "${ENGINEERING_SOURCE_DIR}/test/smoke/${_lib}_smoke.c")
    if(NOT EXISTS "${_src}")
        message(FATAL_ERROR "mm_smoke(${modality}) 缺少源文件: ${_src}")
    endif()

    add_executable(${_exe} "${_src}")
    target_include_directories(${_exe} PRIVATE
        ${ENGINEERING_SOURCE_DIR}/include
        ${ENGINEERING_SOURCE_DIR}/test/smoke
        ${CMAKE_BINARY_DIR}/generated/include
    )
    # 模态 SQL 函数为纯数据表 getter（Task 7 定型）：L1<->L2 无双向符号引用，
    # 依赖图是 DAG，CMake 可拓扑排序（模态库在收集器之后、mm_base 在最后）。
    target_link_libraries(${_exe} PRIVATE
        mm_base
        mm_storage_api
        ${_lib}
    )
    set_target_properties(${_exe} PROPERTIES FOLDER "Smoke")
    add_test(NAME ${_exe} COMMAND ${_exe} WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
endfunction()
