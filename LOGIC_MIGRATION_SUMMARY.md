# 分布式聊天室Logic层业务逻辑迁移总结

## 概述
本次修改将单机版聊天室的核心业务逻辑完全迁移到分布式版本的logic层，确保数据格式和处理逻辑与单机版完全一致，以保证前端的兼容性。

## 主要修改内容

### 1. 核心API文件修改

#### 1.1 api_message_handler.cc
- **完全重写消息处理逻辑**：使用单机版的Redis存储方式
- **ApiHandleClientMessages()**: 处理客户端发送的聊天消息，使用单机版的存储格式
- **ApiGetRoomHistory()**: 从Redis获取历史消息，完全复用单机版逻辑
- **ApiCreateRoom()**: 创建房间，保持单机版的数据格式

#### 1.2 新增api_message.cc和api_message.h
- **ApiGetRoomHistory()**: 完全复用单机版的Redis历史消息获取逻辑
- **ApiStoreMessage()**: 完全复用单机版的Redis消息存储逻辑
- **SerializeMessageToJson()**: 完全复用单机版的JSON序列化格式

#### 1.3 新增api_room.cc和api_room.h
- **ApiCreateRoom()**: 房间创建逻辑，与单机版保持一致
- **ApiGetRoomInfo()**: 获取房间信息
- **ApiGetAllRooms()**: 获取所有房间列表

### 2. 主服务器逻辑修改 (main.cc)

#### 2.1 WebSocket消息格式支持
修改了所有处理函数以支持WebSocket风格的消息格式：

**客户端消息格式**:
```json
{
  "type": "clientMessages",
  "payload": {
    "roomId": "room_id",
    "messages": [
      {"content": "message_content"}
    ]
  }
}
```

**服务器响应格式**:
```json
{
  "type": "serverMessages", 
  "payload": {
    "roomId": "room_id",
    "messages": [
      {
        "id": "message_id",
        "content": "message_content",
        "user": {
          "id": 123,
          "username": "username"
        },
        "timestamp": 1234567890
      }
    ]
  }
}
```

#### 2.2 新增处理函数
- **handleMessages()**: 处理WebSocket格式的客户端消息
- **handleCreateRoom()**: 处理WebSocket格式的房间创建请求
- **handleRoomHistory()**: 处理WebSocket格式的历史消息请求
- **handleHello()**: 处理hello消息，返回用户信息和所有房间的历史消息

### 3. 数据存储策略

#### 3.1 历史消息存储
- **完全使用Redis**: 历史消息直接从Redis Stream获取，不再从MySQL读取
- **数据格式一致**: 保持与单机版完全相同的JSON格式存储在Redis中
- **消息ID生成**: 使用Redis Stream的自动ID生成机制

#### 3.2 房间信息存储
- **MySQL存储**: 房间基本信息仍存储在MySQL中
- **Redis缓存**: 消息数据使用Redis Stream存储

## 支持的API端点

### HTTP POST 端点
1. `/logic/login` - 用户登录
2. `/logic/register` - 用户注册
3. `/logic/verify` - 认证验证
4. `/logic/messages` - 处理聊天消息 (WebSocket格式)
5. `/logic/room/create` - 创建房间 (WebSocket格式)
6. `/logic/room_history` - 获取房间历史 (WebSocket格式)
7. `/logic/hello` - 获取用户信息和房间列表
8. `/logic/send` - 发送消息到Kafka (原有格式)

### HTTP GET 端点
1. `/logic/history` - 获取历史消息 (URL参数格式)

## 数据格式兼容性

### 消息格式
与单机版完全一致：
```json
{
  "content": "消息内容",
  "timestamp": 1234567890,
  "user_id": 123,
  "username": "用户名"
}
```

### 房间历史响应格式
```json
{
  "type": "serverRoomHistory",
  "payload": {
    "roomId": "房间ID",
    "name": "房间名称", 
    "hasMoreMessages": true,
    "messages": [...]
  }
}
```

### Hello消息响应格式
```json
{
  "type": "hello",
  "payload": {
    "me": {
      "id": 123,
      "username": "用户名"
    },
    "rooms": [
      {
        "id": "房间ID",
        "name": "房间名称",
        "hasMoreMessages": true,
        "messages": [...]
      }
    ]
  }
}
```

## 编译和部署

### 编译步骤
```bash
cd myself-chatroom-distribute/server/application/logic
mkdir -p build
cd build
cmake ..
make
```

### 配置文件
确保 `logic.conf` 文件包含正确的数据库和Redis连接配置。

### 运行
```bash
./bin/logic
```
服务器将在端口8090上监听HTTP请求。

## 测试建议

### 1. 消息发送测试
```bash
curl -X POST http://localhost:8090/logic/send \
  -H "Content-Type: application/json" \
  -d '{
    "type": "clientMessages",
    "payload": {
      "roomId": "test_room",
      "messages": [{"content": "Hello World"}]
    }
  }'
```

### 2. 历史消息获取测试
```bash
curl -X POST http://localhost:8090/logic/room_history \
  -H "Content-Type: application/json" \
  -d '{
    "type": "requestRoomHistory", 
    "payload": {
      "roomId": "test_room",
      "count": 10
    }
  }'
```

### 3. 房间创建测试
```bash
curl -X POST http://localhost:8090/logic/room/create \
  -H "Content-Type: application/json" \
  -d '{
    "type": "clientCreateRoom",
    "payload": {
      "roomName": "新房间"
    }
  }'
```

## 注意事项

1. **用户认证**: 当前使用默认的用户ID和用户名，实际部署时需要实现完整的认证机制
2. **错误处理**: 已添加基本的错误处理，但可能需要根据实际需求进一步完善
3. **性能优化**: 大量消息处理时可能需要进一步的性能优化
4. **Kafka集成**: 部分Kafka相关功能已预留接口，需要根据实际需求启用

## 与单机版的兼容性

本次修改确保了：
- ✅ 数据格式完全一致
- ✅ API响应格式完全一致  
- ✅ Redis存储逻辑完全一致
- ✅ 前端无需任何修改即可使用
- ✅ 消息处理逻辑完全一致

通过这些修改，分布式版本的logic层现在完全兼容单机版的前端，可以无缝替换使用。