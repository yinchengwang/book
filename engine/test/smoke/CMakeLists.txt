# 模态独立冒烟测试（Task 7 起每模态一个）
# mm_smoke() 定义见 cmake/mm_smoke.cmake：只链 mm_base + mm_storage_api +
# mm_<modality>，链接期证明模态独立可用；运行期断言只注册了本模态引擎。
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/mm_smoke.cmake)

mm_smoke(kv)
mm_smoke(vector)
mm_smoke(graph)
