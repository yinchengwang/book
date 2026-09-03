.PHONY: run build clean

# 默认目标
all: build

# 构建 Docker 镜像
build:
	docker build -t python-app:latest .

# 运行 docker-compose
up:
	docker-compose up -d

# 停止服务
down:
	docker-compose down

# 查看日志
logs:
	docker-compose logs -f

# 进入容器
shell:
	docker exec -it python-app-web-1 /bin/bash

# 清理未使用的镜像
clean:
	docker image prune -f

# 完整重建
rebuild: down build up
