# --- Build stage ---
# 构建上下文必须是 monorepo 根 (D:/code/book/)，因为 reading-radar 通过
# @shared 别名引用 engineering/apps/games/web/shared/web/src/。
FROM node:20-alpine AS build
WORKDIR /repo
COPY engineering/apps/web/reading-radar/package*.json ./app/
COPY engineering/apps/games/web/shared ./games/web/shared
WORKDIR /repo/app
RUN npm ci
COPY engineering/apps/web/reading-radar . .
RUN npm run build

# --- Runtime stage ---
FROM nginx:alpine
COPY --from=build /repo/app/dist /usr/share/nginx/html
COPY engineering/apps/web/reading-radar/nginx.conf /etc/nginx/conf.d/default.conf
EXPOSE 80
CMD ["nginx", "-g", "daemon off;"]
