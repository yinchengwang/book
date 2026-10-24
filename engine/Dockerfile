# MiniVecDB Dockerfile
# 多阶段构建：编译阶段 + 运行阶段

# ==============================================================================
# 第一阶段：编译
# ==============================================================================
FROM mingw-w64-x86_64 AS builder

# 安装编译依赖
RUN apt-get update && apt-get install -y \
    cmake \
    ninja-build \
    gcc \
    g++ \
    make \
    && rm -rf /var/lib/apt/lists/*

# 设置工作目录
WORKDIR /build

# 复制源码
COPY . /build

# 配置 CMake（交叉编译为 Linux 版本）
RUN cmake -G "Unix Makefiles" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/opt/minivecdb \
    /build

# 编译
RUN cmake --build . --parallel $(nproc)

# 安装
RUN cmake --install .

# ==============================================================================
# 第二阶段：运行（最小化镜像）
# ==============================================================================
FROM alpine:3.19 AS runtime

# 安装运行时依赖
RUN apk add --no-cache \
    ca-certificates \
    libc6-compat \
    libstdc++ \
    libgcc_s1 \
    libatomic1

# 创建数据目录
RUN mkdir -p /data /var/log/minivecdb

# 复制编译产物
COPY --from=builder /opt/minivecdb /opt/minivecdb

# 设置工作目录
WORKDIR /opt/minivecdb

# 设置环境变量
ENV PATH="/opt/minivecdb/bin:${PATH}"
ENV MINIVECDB_DATA_DIR=/data
ENV MINIVECDB_LOG_LEVEL=info

# 健康检查
HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
    CMD wget --no-verbose --tries=1 --spider http://localhost:8080/health || exit 1

# 暴露端口
EXPOSE 8080

# 默认启动命令
ENTRYPOINT ["/opt/minivecdb/bin/AlgorithmPractice", "--mode=server"]
