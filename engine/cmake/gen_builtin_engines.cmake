# ========================================================================
# gen_builtin_engines.cmake —— 生成 mm_builtin_engines.c 并建库
#
# 注册表反转的核心：公共层（core/mm_storage）不再逐个 #ifdef 引用引擎，
# 而是由本文件按 profile 生成一个 mm_builtin_engines.c，集中调用各模态的
# mm_<modality>_register()。
#
# 约束：
#   1. mmdb_generate_builtin_engines() 必须无条件调用 —— 空 profile 也必须
#      产出（空函数体），否则 engine_registry_init 链接失败。
#   2. 生成的文件落在 <build>/generated/src/，不污染源码树。
# ========================================================================

# function() 体内 CMAKE_CURRENT_LIST_DIR 取的是「调用方」所在目录，
# 必须在本文件顶层（include 时）先捕获本目录。
set(MMDB_GEN_BUILTIN_DIR "${CMAKE_CURRENT_LIST_DIR}")

function(mmdb_generate_builtin_engines)
    set(MMDB_ENGINE_REGISTER_CALLS "")
    set(MMDB_SQL_FUNCTION_REGISTER_CALLS "")

    # 模态开关名 -> 注册函数短名（engine_registry.c 的历史命名：doc/ts 非全称）
    set(_short_DOCUMENT doc)
    set(_short_TIMESERIES ts)
    set(_modality_list KV VECTOR GRAPH DOCUMENT TIMESERIES SPATIAL TREE STREAM COLUMNAR ST RDF)
    foreach(_m IN LISTS _modality_list)
        if(MMDB_ENABLE_${_m})
            if(DEFINED _short_${_m})
                set(_low ${_short_${_m}})
            else()
                string(TOLOWER "${_m}" _low)
            endif()
            string(APPEND MMDB_ENGINE_REGISTER_CALLS
                "    (void)mm_${_low}_register();\n")
        endif()
    endforeach()

    # SQL 函数注册：顺序必须与改造前 func_registry_register_builtins 一致
    # （vector -> timeseries -> graph -> document -> spatial -> kv -> tree），
    # 某些函数可能依赖先注册的辅助函数。
    set(_sql_order VECTOR TIMESERIES GRAPH DOCUMENT SPATIAL KV TREE)
    foreach(_m IN LISTS _sql_order)
        if(MMDB_ENABLE_${_m})
            if(DEFINED _short_${_m})
                set(_low ${_short_${_m}})
            else()
                string(TOLOWER "${_m}" _low)
            endif()
            string(APPEND MMDB_SQL_FUNCTION_REGISTER_CALLS
                "    { size_t _n = 0;\n"
                "      const FunctionInfo *_t = mm_${_low}_sql_function_table(&_n);\n"
                "      ret |= func_registry_register_table(_t, _n); }\n")
        endif()
    endforeach()

    set(_generated "${CMAKE_BINARY_DIR}/generated/src/mm_builtin_engines.c")
    configure_file(
        "${MMDB_GEN_BUILTIN_DIR}/mm_builtin_engines.c.in"
        "${_generated}"
        @ONLY
    )

    # 生成文件必须每次构建都产出（空 profile 产出空函数体）。
    # 当前过渡态：mm_<modality>_register 仍定义在 db_core/engine_registry.c，
    # 由调用方把本文件编入 db_core（见 src/db/CMakeLists.txt 的 target_sources）。
    # 若做成独立静态库与 db_core 互链，engine_registry.o 与生成文件的相互引用
    # 会把 CMake 的循环档案重复深度打穿（实测 sql_function_registry_test 链接
    # 因 archive 扫描轮次不足而 undefined）。
    # Task 6 抽出 mm_storage_api 后，mm_*_register 随各模态库提供，
    # 届时再恢复独立的 mm_builtin_engines 静态库（只链 mm_storage_api）。
endfunction()
