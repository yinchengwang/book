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

    set(_modality_list KV VECTOR GRAPH DOCUMENT TIMESERIES SPATIAL TREE STREAM COLUMNAR ST RDF)
    foreach(_m IN LISTS _modality_list)
        if(MMDB_ENABLE_${_m})
            string(TOLOWER "${_m}" _low)
            string(APPEND MMDB_ENGINE_REGISTER_CALLS
                "    (void)mm_${_low}_register();\n")
            # Task 3 拆分 func_registry 后，这里同步追加
            # (void)mm_${_low}_register_sql_functions();
        endif()
    endforeach()

    set(_generated "${CMAKE_BINARY_DIR}/generated/src/mm_builtin_engines.c")
    configure_file(
        "${MMDB_GEN_BUILTIN_DIR}/mm_builtin_engines.c.in"
        "${_generated}"
        @ONLY
    )

    add_library(mm_builtin_engines STATIC "${_generated}")
    target_include_directories(mm_builtin_engines PUBLIC
        "${ENGINEERING_SOURCE_DIR}/include"
    )
    # 当前过渡态：mm_<modality>_register 仍定义在 db_core/engine_registry.c，
    # register_storage_engine 由 db_storage 提供，故先链 db_core。
    # Task 6 抽出 mm_storage_api 后，本库改链 mm_storage_api，
    # mm_*_register 随各模态库由 profile 链接闭包提供。
    target_link_libraries(mm_builtin_engines PUBLIC db_core)
    set_target_properties(mm_builtin_engines PROPERTIES FOLDER "Generated")
endfunction()
