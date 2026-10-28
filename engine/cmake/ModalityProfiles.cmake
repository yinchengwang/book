# ========================================================================
# ModalityProfiles.cmake —— MMDB_MODALITY 单一 profile 入口
#
# 用法：cmake -B build/x -S . -G Ninja -DMMDB_MODALITY=<profile>
#
# 职责：
#   1. 定义 profile 表（单模态 13 个 + legacy/full 组合）；
#   2. 按 profile 推导全部 MMDB_ENABLE_* 缓存变量（先于
#      MultimodalConfig.cmake 的 option() 声明执行，option() 见缓存已存在
#      会保留推导值）；
#   3. 配置期闭包校验：依赖不封闭直接 FATAL_ERROR；
#   4. 拦截手动开关：全新配置下出现显式 -DMMDB_ENABLE_* 而未加
#      -DMMDB_ALLOW_MANUAL_SWITCHES=ON 时拒绝；
#   5. MMDB_PROFILE_APPLIED 幂等守卫：同一构建树换 profile 要求换目录。
#
# 必须在 include(MultimodalConfig.cmake) 之前 include。
# ========================================================================

# profile 名 -> 模态列表。注意 profile 名与 MMDB_ENABLE_* 后缀并非一一对应：
#   tree  -> MMDB_ENABLE_TREE（yang 引擎）
#   doc   -> MMDB_ENABLE_DOCUMENT
#   ts    -> MMDB_ENABLE_TIMESERIES
#   st    -> MMDB_ENABLE_ST（时空引擎）
set(MMDB_PROFILE_kv         "kv")
set(MMDB_PROFILE_vector     "vector")
set(MMDB_PROFILE_graph      "graph;kv")
set(MMDB_PROFILE_spatial    "spatial")
set(MMDB_PROFILE_tree       "tree")
set(MMDB_PROFILE_sparse     "sparse;vector")
set(MMDB_PROFILE_doc        "doc;vector")
set(MMDB_PROFILE_ts         "ts")
set(MMDB_PROFILE_stream     "stream")
set(MMDB_PROFILE_columnar   "columnar")
set(MMDB_PROFILE_st         "st;spatial")
set(MMDB_PROFILE_rdf        "rdf")
set(MMDB_PROFILE_relational "relational")
set(MMDB_PROFILE_legacy     "relational;kv;graph;vector;spatial;tree;sparse")
set(MMDB_PROFILE_full       "relational;kv;graph;vector;spatial;tree;sparse;doc;ts;stream;columnar;st;rdf")

# 依赖闭包（硬编码表）：key 依赖 value 列表，启用 key 必须同时启用 value
set(MMDB_MODALITY_DEPS_graph   "kv")
set(MMDB_MODALITY_DEPS_st      "spatial")
set(MMDB_MODALITY_DEPS_sparse  "vector")
set(MMDB_MODALITY_DEPS_doc     "vector")

# ------------------------------------------------------------------
# 模态 -> 引擎注册槽位（用于冒烟断言的 profile 感知）
#
# 冒烟测试原先硬编码「注册表里只有 N 个引擎」，这只在单模态 profile
# 下成立；legacy/full profile 会注册多个引擎，导致冒烟必失败。
# 这里把每个模态是否注册 storage 引擎、注册到哪个 DataModel 槽位
# 显式列出，构建期据此算出本 profile 的期望引擎位掩码，
# 生成 mm_smoke_expect.h 供冒烟断言使用。
#
# 位序 = DataModel 枚举值（include/db/storage_engine.h）：
#   RELATIONAL=0 KV=1 GRAPH=2 VECTOR=3 TIMESERIES=4 DOCUMENT=5
#   SPATIAL=6 TREE=7 STREAM=8 COLUMNAR=9 RDF=11 SPATIOTEMPORAL=12
# relational / sparse 不实现 storage_ops_t（无引擎注册），故为 0。
# ------------------------------------------------------------------
set(MMDB_MODALITY_ENGINE_relational   0)
set(MMDB_MODALITY_ENGINE_kv           2)     # 1 << MODEL_KV
set(MMDB_MODALITY_ENGINE_vector       8)     # 1 << MODEL_VECTOR
set(MMDB_MODALITY_ENGINE_graph        4)     # 1 << MODEL_GRAPH
set(MMDB_MODALITY_ENGINE_doc          32)    # 1 << MODEL_DOCUMENT
set(MMDB_MODALITY_ENGINE_ts           16)    # 1 << MODEL_TIMESERIES
set(MMDB_MODALITY_ENGINE_spatial      64)    # 1 << MODEL_SPATIAL
set(MMDB_MODALITY_ENGINE_tree         128)   # 1 << MODEL_TREE
set(MMDB_MODALITY_ENGINE_stream       256)   # 1 << MODEL_STREAM
set(MMDB_MODALITY_ENGINE_columnar     512)   # 1 << MODEL_COLUMNAR
set(MMDB_MODALITY_ENGINE_rdf          2048)  # 1 << MODEL_RDF
set(MMDB_MODALITY_ENGINE_st           4096)  # 1 << MODEL_SPATIOTEMPORAL
set(MMDB_MODALITY_ENGINE_sparse       0)

# profile 模态名 -> MMDB_ENABLE_* 开关后缀映射（显式列出，防止改名漂移）
set(MMDB_MODALITY_SWITCH_relational "RELATIONAL")
set(MMDB_MODALITY_SWITCH_kv         "KV")
set(MMDB_MODALITY_SWITCH_vector     "VECTOR")
set(MMDB_MODALITY_SWITCH_graph      "GRAPH")
set(MMDB_MODALITY_SWITCH_doc        "DOCUMENT")
set(MMDB_MODALITY_SWITCH_ts         "TIMESERIES")
set(MMDB_MODALITY_SWITCH_spatial    "SPATIAL")
set(MMDB_MODALITY_SWITCH_tree       "TREE")
set(MMDB_MODALITY_SWITCH_stream     "STREAM")
set(MMDB_MODALITY_SWITCH_columnar   "COLUMNAR")
set(MMDB_MODALITY_SWITCH_st         "ST")
set(MMDB_MODALITY_SWITCH_rdf        "RDF")
set(MMDB_MODALITY_SWITCH_sparse     "SPARSE")

set(MMDB_ALL_MODALITY_SWITCHES
    RELATIONAL KV GRAPH VECTOR TIMESERIES DOCUMENT SPATIAL TREE
    STREAM COLUMNAR ST RDF SPARSE)

# ------------------------------------------------------------------
# 校验 MMDB_MODALITY
# ------------------------------------------------------------------
set(MMDB_MODALITY "legacy" CACHE STRING
    "Modality build profile (see MMDB_PROFILE_* in cmake/ModalityProfiles.cmake)")

if(NOT DEFINED MMDB_PROFILE_${MMDB_MODALITY})
    message(FATAL_ERROR
        "未知的 MMDB_MODALITY='${MMDB_MODALITY}'。可用 profile: "
        "kv vector graph spatial tree sparse doc ts stream columnar st rdf "
        "relational legacy full")
endif()

# ------------------------------------------------------------------
# 本 profile 的期望引擎位掩码（按 profile 模态列表并集）
#
# 供冒烟测试断言用：注册表中的非空槽位集合必须恰好等于本掩码，
# 多出来一个即说明有引擎在不该存在的 profile 下被注册。
# ------------------------------------------------------------------
set(MMDB_EXPECTED_ENGINE_MASK 0)
foreach(_m IN LISTS MMDB_PROFILE_${MMDB_MODALITY})
    math(EXPR MMDB_EXPECTED_ENGINE_MASK
         "${MMDB_EXPECTED_ENGINE_MASK} | ${MMDB_MODALITY_ENGINE_${_m}}")
endforeach()
message(STATUS "Modality profile '${MMDB_MODALITY}': expected engine mask = ${MMDB_EXPECTED_ENGINE_MASK}")

#
# 「用户显式给了 -DMMDB_ENABLE_*」只能用 DEFINED CACHE{...} 判断
# （if(MMDB_ENABLE_KV) 在 option() 声明后恒真）。为区分「用户 -D」与
# 「上次配置的推导值」，用 MMDB_PROFILE_APPLIED 哨兵：
#   全新配置（无哨兵）出现显式开关且未放行 -> FATAL；
#   重配置（有哨兵）-> profile 必须一致，推导值属本机制所有，放行。
# ------------------------------------------------------------------
if(DEFINED CACHE{MMDB_PROFILE_APPLIED})
    if(NOT MMDB_PROFILE_APPLIED STREQUAL MMDB_MODALITY)
        message(FATAL_ERROR
            "该构建树已应用 profile '${MMDB_PROFILE_APPLIED}'，改用 "
            "'${MMDB_MODALITY}' 请换构建目录（profile 决定编译闭包，原地切换会产生陈旧产物）")
    endif()
    # 哨兵匹配但仍须拒绝「与 profile 相悖的显式 ON 开关」：同 profile 重配置时
    # 新传的 -DMMDB_ENABLE_X=ON 会落在缓存里，若静默接受会让 profile 形同虚设
    #（推导 FORCE 会把它翻回 OFF，用户意图被无声吞掉）。手动放行时不在此拦截，
    # 交给闭包校验。
    if(NOT MMDB_ALLOW_MANUAL_SWITCHES)
        set(_expected_on "")
        foreach(_m IN LISTS MMDB_PROFILE_${MMDB_MODALITY})
            list(APPEND _expected_on ${MMDB_MODALITY_SWITCH_${_m}})
        endforeach()
        foreach(_s IN LISTS MMDB_ALL_MODALITY_SWITCHES)
            if(MMDB_ENABLE_${_s})
                list(FIND _expected_on ${_s} _i)
                if(_i EQUAL -1)
                    message(FATAL_ERROR
                        "本构建树已应用 profile '${MMDB_MODALITY}'，但检测到 "
                        "MMDB_ENABLE_${_s}=ON（与 profile 相悖）。请去掉该 -D，"
                        "或换构建目录，或用 -DMMDB_ALLOW_MANUAL_SWITCHES=ON 显式放行。")
                endif()
            endif()
        endforeach()
    endif()
else()
    set(_manual_switch_detected OFF)
    foreach(_s IN LISTS MMDB_ALL_MODALITY_SWITCHES)
        if(DEFINED CACHE{MMDB_ENABLE_${_s}})
            set(_manual_switch_detected ON)
            break()
        endif()
    endforeach()
    if(_manual_switch_detected AND NOT MMDB_ALLOW_MANUAL_SWITCHES)
        message(FATAL_ERROR
            "检测到显式 -DMMDB_ENABLE_*：请改用 -DMMDB_MODALITY=<profile>；"
            "确需手动组合请加 -DMMDB_ALLOW_MANUAL_SWITCHES=ON")
    endif()
endif()

# ------------------------------------------------------------------
# 按 profile 推导全部模态开关（幂等，可重复配置）
# ------------------------------------------------------------------
set(MMDB_ENABLED_MODALITIES "")

# MMDB_ALLOW_MANUAL_SWITCHES=ON 时，用户显式 -D 的开关值优先于 profile 推导；
# 闭包校验仍然生效（手动关掉依赖模态会被 FATAL_ERROR 拒绝）。
foreach(_s IN LISTS MMDB_ALL_MODALITY_SWITCHES)
    if(MMDB_ALLOW_MANUAL_SWITCHES AND DEFINED CACHE{MMDB_ENABLE_${_s}})
        continue()
    endif()
    set(MMDB_ENABLE_${_s} OFF CACHE BOOL
        "Derived from MMDB_MODALITY='${MMDB_MODALITY}' (do not set manually)" FORCE)
endforeach()

foreach(_m IN LISTS MMDB_PROFILE_${MMDB_MODALITY})
    if(NOT DEFINED MMDB_MODALITY_SWITCH_${_m})
        message(FATAL_ERROR "profile '${MMDB_MODALITY}' 含未知模态 '${_m}'")
    endif()
    set(_sw ${MMDB_MODALITY_SWITCH_${_m}})
    if(MMDB_ALLOW_MANUAL_SWITCHES AND DEFINED CACHE{MMDB_ENABLE_${_sw}})
        # 用户手动值优先；若与 profile 相悖，交给下方闭包校验裁决
    else()
        set(MMDB_ENABLE_${_sw} ON CACHE BOOL
            "Derived from MMDB_MODALITY='${MMDB_MODALITY}' (do not set manually)" FORCE)
    endif()
    list(APPEND MMDB_ENABLED_MODALITIES ${_m})
endforeach()

# ------------------------------------------------------------------
# 依赖闭包校验（看实际开关值，而非 profile 列表成员 —— 手动覆盖后两者会分叉）
# ------------------------------------------------------------------
foreach(_m IN LISTS MMDB_ENABLED_MODALITIES)
    if(DEFINED MMDB_MODALITY_DEPS_${_m})
        foreach(_dep IN LISTS MMDB_MODALITY_DEPS_${_m})
            if(NOT DEFINED MMDB_MODALITY_SWITCH_${_dep})
                message(FATAL_ERROR "依赖闭包表含未知模态 '${_dep}'")
            endif()
            set(_dep_sw ${MMDB_MODALITY_SWITCH_${_dep}})
            if(NOT MMDB_ENABLE_${_dep_sw})
                message(FATAL_ERROR
                    "profile '${MMDB_MODALITY}' 闭包不封闭：模态 '${_m}' 依赖 "
                    "'${_dep}'（MMDB_ENABLE_${_dep_sw}），但后者未启用。"
                    "请补全 MODALITIES 列表或移除手动覆盖。")
            endif()
        endforeach()
    endif()
endforeach()

list(LENGTH MMDB_ENABLED_MODALITIES MMDB_ENABLED_MODALITY_COUNT)

# 哨兵最后落缓存：以上全部通过才视为「该 profile 已应用于本构建树」
set(MMDB_PROFILE_APPLIED "${MMDB_MODALITY}" CACHE INTERNAL
    "Profile applied to this build tree (do not edit)" FORCE)

message(STATUS "MMDB_MODALITY = ${MMDB_MODALITY} "
    "(${MMDB_ENABLED_MODALITY_COUNT} modalities: ${MMDB_ENABLED_MODALITIES})")
