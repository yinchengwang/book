# 模态独立冒烟测试（Task 7 起每模态一个）
# mm_smoke() 定义见 cmake/mm_smoke.cmake：只链 mm_base + mm_storage_api +
# mm_<modality>，链接期证明模态独立可用；运行期断言只注册了本模态引擎。
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/mm_smoke.cmake)

# 按目标存在性守卫：模态关闭时其库不存在，冒烟目标也不应定义
# （否则退化为裸 -l 链接失败；且其 n==1 断言在本 profile 下本就不成立）
if(TARGET mm_kv)
    mm_smoke(kv)
endif()
if(TARGET mm_vector)
    mm_smoke(vector)
endif()
if(TARGET mm_graph)
    mm_smoke(graph)
endif()
if(TARGET mm_spatial)
    mm_smoke(spatial)
endif()
if(TARGET mm_yang)
    mm_smoke(tree LIB mm_yang)
endif()
if(TARGET mm_sparse)
    mm_smoke(sparse LIB mm_sparse)
endif()
if(TARGET mm_stream)
    mm_smoke(stream)
endif()
if(TARGET mm_columnar)
    mm_smoke(columnar)
endif()
if(TARGET mm_doc)
    mm_smoke(doc LIB mm_doc)
endif()
