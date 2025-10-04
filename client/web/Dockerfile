# 前端Dockerfile - 生产环境构建
FROM node:21.7.3-alpine AS builder

WORKDIR /app

# 复制package文件
COPY package*.json ./
COPY package-lock.json ./

# 安装依赖
RUN npm ci --only=production

# 复制源代码
COPY . .

# 构建应用
RUN NEXT_TELEMETRY_DISABLED=1 npm run build

# 生产环境镜像
FROM nginx:alpine

# 复制构建结果到nginx目录
COPY --from=builder /app/out /usr/share/nginx/html

# 复制nginx配置
COPY nginx.conf /etc/nginx/nginx.conf

# 暴露端口
EXPOSE 80

# 启动nginx
CMD ["nginx", "-g", "daemon off;"]