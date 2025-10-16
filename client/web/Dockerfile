# 使用 Next.js 服务器运行（无静态导出），保留 rewrites 能力
# 阶段1：依赖安装与构建
FROM node:18-alpine AS builder

WORKDIR /app

# 仅拷贝依赖清单，利用缓存
COPY package*.json ./

# npm ci 进行依赖安装
RUN npm ci --registry https://registry.npmmirror.com --retry 5 --retry-delay 3

# 拷贝源码
COPY . .

# 生产构建
ARG SERVER_BASE_URL
ARG NEXT_PUBLIC_API_URL
ARG NEXT_PUBLIC_WS_URL
ENV NODE_ENV=production NEXT_TELEMETRY_DISABLED=1 SERVER_BASE_URL=$SERVER_BASE_URL NEXT_PUBLIC_API_URL=$NEXT_PUBLIC_API_URL NEXT_PUBLIC_WS_URL=$NEXT_PUBLIC_WS_URL
RUN npm run build

# 阶段2：运行期，使用 next start 启动 Next 服务
FROM node:18-alpine AS runner

WORKDIR /app
ENV NODE_ENV=production NEXT_TELEMETRY_DISABLED=1

# 拷贝运行所需文件与构建产物
COPY --from=builder /app/package*.json ./
COPY --from=builder /app/next.config.js ./next.config.js
COPY --from=builder /app/.next ./.next
COPY --from=builder /app/public ./public
COPY --from=builder /app/node_modules ./node_modules

# 暴露 Next 默认端口
EXPOSE 3000

# 启动 Next 服务器
CMD ["npm", "run", "start"]