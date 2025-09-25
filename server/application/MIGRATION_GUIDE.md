# 分布式聊天室用户认证迁移指南

## 🎯 迁移目标
将用户认证和会话管理从comet层（chatroom）迁移到logic层，实现真正的分层架构。

## ✅ 已完成的工作

### 1. Logic层用户认证API实现
- **api_login.h/cc**: 用户登录功能，支持邮箱+密码登录
- **api_register.h/cc**: 用户注册功能，支持用户名+邮箱+密码注册
- **api_auth.h/cc**: 用户认证验证功能，通过cookie验证用户身份
- **更新main.cc**: 添加HTTP接口处理
- **更新CMakeLists.txt**: 包含新的源文件

### 2. 新增的HTTP接口
- `POST /logic/login` - 用户登录
- `POST /logic/register` - 用户注册  
- `POST /logic/verify` - 验证用户认证状态

### 3. Comet层修改
- **logic_client.h/cc**: HTTP客户端，用于调用logic层认证API
- **logic_config.h/cc**: 配置管理，支持可配置的logic服务器地址
- **修改websocket_conn.cc**: 将直接认证改为调用logic层API
- **更新chat-room.conf**: 添加logic服务器配置
- **更新CMakeLists.txt**: 添加curl库依赖

## 🔧 配置说明

### Logic层配置 (logic.conf)
```ini
# HTTP服务配置
http_bind_ip=0.0.0.0
http_bind_port=8090

# 数据库和Redis配置保持不变
```

### Comet层配置 (chat-room.conf)
```ini
# Logic服务器配置
logic_server_host=localhost
logic_server_port=8090
logic_connect_timeout=3
logic_request_timeout=5
```

## 🚀 部署和测试

### 1. 编译项目
```bash
cd /home/utopianyouth/zerovoice/unit11/myself-chatroom-distribute
mkdir -p build && cd build
cmake .. -DENABLE_RPC=ON
make -j4
```

### 2. 启动服务
```bash
# 启动logic服务
cd server/application/logic
./logic logic.conf

# 启动chat-room服务 (comet层)
cd server/application/chat-room  
./chat-room chat-room.conf

# 启动job服务
cd server/application/job
./job
```

### 3. 测试认证接口

#### 测试注册
```bash
curl -X POST http://localhost:8090/logic/register \
  -H "Content-Type: application/json" \
  -d '{"username":"testuser","email":"test@example.com","password":"123456"}'
```

#### 测试登录
```bash
curl -X POST http://localhost:8090/logic/login \
  -H "Content-Type: application/json" \
  -d '{"email":"test@example.com","password":"123456"}'
```

#### 测试认证验证
```bash
curl -X POST http://localhost:8090/logic/verify \
  -H "Content-Type: application/json" \
  -d '{"cookie":"your-cookie-here"}'
```

### 4. 测试WebSocket连接
```javascript
// 前端测试代码
const ws = new WebSocket('ws://localhost:8080');
// WebSocket握手时会自动调用logic层进行认证验证
```

## 📋 迁移后的架构优势

### Logic层职责：
- ✅ 用户认证和会话管理
- ✅ 消息合法性校验
- ✅ 数据持久化和查询
- ✅ Kafka消息发布

### Comet层职责：
- ✅ WebSocket连接管理
- ✅ 消息转发到logic层
- ✅ 接收job层推送并广播给客户端
- ✅ 水平扩展支持

### Job层职责：
- ✅ Kafka消息消费
- ✅ 用户在线状态查询
- ✅ gRPC调用comet节点推送消息

## 🔄 数据流程

### 用户登录流程：
1. 客户端 → Logic层 (`POST /logic/login`)
2. Logic层验证用户名密码
3. Logic层生成cookie并存储到Redis
4. 返回cookie给客户端

### WebSocket连接认证流程：
1. 客户端发起WebSocket连接到Comet层
2. Comet层提取cookie
3. Comet层 → Logic层 (`POST /logic/verify`)
4. Logic层验证cookie并返回用户信息
5. Comet层建立WebSocket连接

### 消息发送流程：
1. 客户端 → Comet层 (WebSocket)
2. Comet层 → Logic层 (HTTP)
3. Logic层存储消息并发送到Kafka
4. Job层消费Kafka消息
5. Job层 → Comet层 (gRPC)
6. Comet层广播给相关客户端

## 📝 注意事项

1. **数据一致性**：确保Redis中的session数据在所有logic节点间共享
2. **错误处理**：添加网络调用的重试和降级机制
3. **性能监控**：监控logic层的认证接口性能
4. **安全性**：确保内部服务间通信的安全性
5. **负载均衡**：为logic层配置负载均衡器支持水平扩展

## 🐛 故障排除

### 常见问题：
1. **连接失败**：检查logic服务是否启动，端口是否正确
2. **认证失败**：检查Redis连接，确保session数据正确
3. **编译错误**：确保安装了curl开发库 (`sudo apt-get install libcurl4-openssl-dev`)

### 日志查看：
```bash
# 查看logic层日志
tail -f logic.log

# 查看comet层日志  
tail -f chat-room.log
```

这个迁移方案实现了真正的分布式架构，每层职责清晰，便于水平扩展和维护。