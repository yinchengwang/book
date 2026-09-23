# RPC 模块
# db_rpc：分布式 RPC 通信库
# 注：rpc_client.c / rpc_server.c 历史上未被任何目标编译（连接池实现，
#     存在 bit-rot 风险），本库当前仅收录 Gap#4 Task 9 的 rpc_stream.c；
#     旧文件保留在目录中暂不构建。
add_library(db_rpc STATIC rpc_stream.c)
target_include_directories(db_rpc PUBLIC
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/include/db
)

# Windows 需要链接 ws2_32（参照 db_core 的做法；
# 静态库的 PRIVATE 链接依赖会以 $LINK_ONLY 形式传播给消费目标）
if(WIN32)
    target_link_libraries(db_rpc PRIVATE ws2_32)
endif()
