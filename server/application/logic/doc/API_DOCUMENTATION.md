# Logic服务API文档

## 概述
Logic服务现在支持消息持久化存储功能，在发送消息到Kafka的同时，会将消息存储到MySQL数据库和Redis缓存中，以便后续查询历史消息。

## 服务配置
- **端口**: 8090
- **配置文件**: logic.conf
- **数据库**: myself_chatroom.messages表
- **缓存**: Redis Stream

## API接口

### 1. 发送消息 (已有功能增强)
**接口**: `POST /logic/send`
**功能**: 发送消息到Kafka队列，同时存储到数据库和缓存

**请求示例**:
```json
{
    "roomId": "room_001",
    "userId": 12345,
    "userName": "张三",
    "messages": [
        {
            "content": "Hello, World!"
        },
        {
            "content": "这是第二条消息"
        }
    ]
}
```

**响应示例**:
```json
{
    "status": "success"
}
```

**功能增强**:
- ✅ 消息同时存储到MySQL数据库
- ✅ 消息同时存储到Redis缓存
- ✅ 自动生成唯一消息ID
- ✅ 记录时间戳

### 2. 获取历史消息 (新增功能)
**接口**: `GET /logic/history`
**功能**: 分页获取房间历史消息

**请求参数**:
- `room_id` (必需): 房间ID
- `limit` (可选): 每页消息数量，默认20
- `last_id` (可选): 上次获取的最后一条消息ID，用于分页

**请求示例**:
```
GET /logic/history?room_id=room_001&limit=10
GET /logic/history?room_id=room_001&limit=10&last_id=1695123456789-uuid
```

**响应示例**:
```json
{
    "success": true,
    "room_id": "room_001",
    "has_more": true,
    "messages": [
        {
            "id": "1695123456789-uuid-1",
            "user_id": 12345,
            "username": "张三",
            "content": "Hello, World!",
            "timestamp": 1695123456789
        },
        {
            "id": "1695123456790-uuid-2",
            "user_id": 12346,
            "username": "李四",
            "content": "你好！",
            "timestamp": 1695123456790
        }
    ]
}
```

### 3. 用户登录 (已有功能)
**接口**: `POST /logic/login`
**功能**: 简单的登录处理

**响应示例**:
```json
{
    "status": "finished login in"
}
```

## 数据库表结构

### messages表
```sql
CREATE TABLE messages (
    id BIGINT AUTO_INCREMENT PRIMARY KEY,
    message_id VARCHAR(64) NOT NULL UNIQUE,
    room_id VARCHAR(64) NOT NULL,
    user_id BIGINT NOT NULL,
    username VARCHAR(100) NOT NULL,
    content TEXT NOT NULL,
    timestamp BIGINT NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    INDEX idx_room_timestamp (room_id, timestamp),
    INDEX idx_message_id (message_id)
);
```

## ApiPost测试用例

### 测试1: 发送消息
```
POST http://localhost:8090/logic/send
Content-Type: application/json

{
    "roomId": "test_room_001",
    "userId": 1001,
    "userName": "测试用户1",
    "messages": [
        {
            "content": "这是一条测试消息"
        },
        {
            "content": "这是第二条测试消息"
        }
    ]
}
```

### 测试2: 获取历史消息
```
GET http://localhost:8090/logic/history?room_id=test_room_001&limit=5
```

### 测试3: 分页获取历史消息
```
GET http://localhost:8090/logic/history?room_id=test_room_001&limit=5&last_id=上次返回的最后一条消息ID
```

## 启动服务

1. 确保MySQL和Redis服务正在运行
2. 确保Kafka服务正在运行
3. 启动logic服务:
```bash
cd server/bin
./logic ../application/logic/logic.conf
```

## 注意事项

1. **消息持久化**: 每条发送的消息都会同时存储到MySQL和Redis中
2. **消息ID**: 系统自动生成唯一的消息ID，格式为"时间戳-UUID"
3. **分页查询**: 使用timestamp和id的组合进行分页，确保消息顺序正确
4. **错误处理**: 即使数据库存储失败，Kafka消息仍会正常发送
5. **性能考虑**: Redis用于快速查询，MySQL用于持久化存储

## 集成说明

现在Logic层具备了完整的消息持久化功能，可以：
- 接收前端发送的消息
- 将消息发送到Kafka进行分布式处理
- 同时将消息存储到数据库和缓存
- 提供历史消息查询接口

这样就实现了您要求的"在Logic层存储消息的方案"，为后续在Comet层扩展查询聊天历史记录功能奠定了基础。