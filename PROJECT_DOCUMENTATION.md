# 分布式聊天室项目详细说明文档

本文档详细介绍了该分布式聊天室项目的架构、协议、部署和数据流。

## 第一部分：依赖、环境与部署

### 1.1 所需依赖和环境

#### 前端 (client/web)

- **Node.js**: >= 18.x
- **npm**: >= 9.x
- **依赖包**:
  - `next`, `react`, `react-dom`: 核心框架。
  - `axios`: 用于HTTP请求。
  - `crypto-js`, `md5`: 用于加密。
  - `@mui/material`, `tailwindcss`: UI组件库和样式。
  - `jest`, `@testing-library/react`: 测试框架。

#### 后端 (server)

- **操作系统**: Linux (例如 Ubuntu 20.04+)
- **编译器**: g++ (>= 9.x)
- **构建工具**: CMake (>= 3.16), make
- **核心库**:
  - **Muduo**: 基于Reactor模式的C++网络库，用于处理TCP连接。
  - **gRPC**: 用于后端服务间的RPC通信。
  - **Protocol Buffers**: 用于定义服务间通信的数据结构。
  - **librdkafka**: 用于与Kafka进行交互。
  - **hiredis**: 用于与Redis进行交互。
  - **jsoncpp**: 用于解析和生成JSON。
- **数据库和服务**:
  - **MySQL**: (>= 8.0) 用于持久化存储用户信息、房间信息和历史消息。
  - **Redis**: (>= 6.x) 用于缓存Session（Token）和部分消息数据。
  - **Kafka**: (>= 3.x) 作为消息队列，用于解耦业务逻辑和消息推送。
- **容器化**:
  - **Docker**: 用于构建和运行前后端服务的镜像。

### 1.2 部署与运行步骤

#### 前端

1.  **进入前端目录**:
    ```bash
    cd client/web
    ```
2.  **安装依赖**:
    ```bash
    npm install
    ```
3.  **启动开发环境**:
    ```bash
    npm run dev
    ```
    应用将在 `http://localhost:3000` 启动。

4.  **构建生产版本**:
    ```bash
    npm run build
    ```
5.  **启动生产服务器**:
    ```bash
    npm run start
    ```

#### 后端

1.  **环境准备**:
    确保已安装`g++`, `cmake`, `make`以及所有数据库和服务（MySQL, Redis, Kafka）。

2.  **初始化数据库**:
    使用 `server/create_tables.sql` 脚本在MySQL中创建所需的数据库和表。
    ```bash
    mysql -u your_user -p < server/create_tables.sql
    ```

3.  **构建后端服务**:
    进入 `server` 目录，通常会有一个构建脚本，或者使用`cmake`和`make`手动构建。
    ```bash
    cd server
    mkdir build
    cd build
    cmake ..
    make
    ```
    编译后，可执行文件（`chat-room`, `logic`, `job`）会生成在 `server/build/bin` 目录下。

4.  **运行后端服务**:
    需要按顺序启动三个核心服务：

    - **启动 Logic 服务**:
      ```bash
      ./server/build/bin/logic
      ```
      该服务监听 `8090` 端口，处理核心业务逻辑。

    - **启动 Job 服务**:
      ```bash
      ./server/build/bin/job
      ```
      该服务连接到Kafka，消费消息并将其通过gRPC推送给Comet层。

    - **启动 Comet (chat-room) 服务**:
      ```bash
      ./server/build/bin/chat-room
      ```
      该服务监听 `8080` 端口（HTTP/WebSocket）和 `50051` 端口（gRPC），处理客户端连接和消息推送。

## 第二部分：数据格式协议解析

项目采用两种主要的数据格式：**Protocol Buffers (Protobuf)** 用于后端内部服务间的高效通信，**JSON** 用于前后端之间的HTTP/WebSocket通信。

### 2.1 后端内部通信协议 (Protobuf)

#### `ChatRoom.Protocol.proto`
定义了所有消息的基础信封（Envelope），所有具体的消息内容都包装在 `body` 字段中。

- `Proto`:
  - `ver`: 协议版本。
  - `op`: 操作码，用于区分消息类型。
  - `seq`: 序列号，用于请求-响应匹配。
  - `body`: 实际的消息内容，通常是序列化后的另一个Protobuf消息或JSON字符串。

#### `ChatRoom.Comet.proto`
定义了 `Job` 层与 `Comet` 层之间的gRPC服务接口。

- **Service `Comet`**:
  - `PushMsg`: 向指定的一个或多个用户推送消息。
  - `Broadcast`: 向所有在线用户广播消息（如系统公告）。
  - `BroadcastRoom`: 向指定房间内的所有用户广播消息。
  - `Rooms`: 获取所有房间列表（`Job`层不使用，可能为其他服务预留）。

#### `ChatRoom.Job.proto` (Logic -> Kafka)
定义了 `Logic` 层发送到 Kafka 的消息格式。

- `PushMsg`:
  - `type`: 消息类型 (PUSH, ROOM, BROADCAST)。
  - `operation`: 操作码，与 `ChatRoom.Protocol.proto` 中的 `op` 对应。
  - `room`: 目标房间ID。
  - `keys`: 目标用户ID列表。
  - `msg`: 消息体，通常是序列化后的JSON字符串，用于前端解析。

### 2.2 前后端通信协议 (JSON)

#### 用户注册
- **Request** (`POST /logic/register`):
  ```json
  {
    "username": "testuser",
    "email": "test@example.com",
    "password": "hashed_password"
  }
  ```
- **Response** (Success):
  - `Set-Cookie`: `sid=...`
  - `Body`:
    ```json
    {
      "status": "success",
      "userId": "user_id_string"
    }
    ```

#### 用户登录
- **Request** (`POST /logic/login`):
  ```json
  {
    "email": "test@example.com",
    "password": "hashed_password"
  }
  ```
- **Response** (Success):
  - `Set-Cookie`: `sid=...`
  - `Body`:
    ```json
    {
      "status": "success",
      "userId": "user_id_string",
      "username": "testuser"
    }
    ```

#### 用户认证 (WebSocket连接后)
- **Request** (`POST /logic/verify`):
  ```json
  {
    "token": "session_id_from_cookie"
  }
  ```
- **Response** (Success):
  ```json
  {
    "status": "success",
    "userId": "user_id_string",
    "username": "testuser"
  }
  ```

#### 发送聊天信息
- **Request** (`POST /logic/send`):
  ```json
  {
    "roomId": "room_123",
    "userId": "user_456",
    "username": "testuser",
    "messages": [
      { "content": "Hello, world!" }
    ]
  }
  ```
- **Response** (Success):
  ```json
  { "status": "success" }
  ```

#### 房间内广播 (由后端推送)
- **Push Message** (WebSocket):
  ```json
  {
    "type": "serverMessages",
    "payload": {
      "roomId": "room_123",
      "messages": [
        {
          "id": "msg_789",
          "content": "Hello, world!",
          "user": { "id": "user_456", "username": "testuser" },
          "timestamp": 1678886400000
        }
      ]
    }
  }
  ```

#### 查询历史消息
- **Request** (`POST /logic/room_history`):
  ```json
  {
    "type": "requestRoomHistory",
    "payload": {
      "roomId": "room_123",
      "firstMessageId": "optional_last_message_id",
      "count": 20
    }
  }
  ```
- **Response** (Success):
  ```json
  {
    "type": "serverRoomHistory",
    "payload": {
      "roomId": "room_123",
      "hasMoreMessages": true,
      "messages": [ /* ... message list ... */ ]
    }
  }
  ```

#### 创建房间
- **Request** (`POST /logic/room/create`):
  ```json
  {
    "type": "clientCreateRoom",
    "payload": {
      "roomName": "My New Room",
      "creatorId": "user_456",
      "creatorUsername": "testuser"
    }
  }
  ```
- **Response** (Success):
  ```json
  {
    "type": "serverCreateRoom",
    "payload": {
      "roomId": "new_room_id",
      "roomName": "My New Room",
      /* ... creator info ... */
    }
  }
  ```

#### 创建房间的全局广播 (由后端推送)
- **Push Message** (WebSocket):
  ```json
  {
    "type": "serverCreateRoom",
    "payload": {
      "roomId": "new_room_id",
      "roomName": "My New Room",
      /* ... creator info ... */
    }
  }
  ```

## 第三部分：后端架构详细分析

项目的后端遵循分层架构，各层职责明确，通过gRPC和Kafka进行通信。

### 3.1 聊天室后端全局架构图

```mermaid
graph TD
    subgraph "客户端"
        Client[Web Client]
    end

    subgraph "后端服务"
        subgraph "Comet 层 (chat-room)"
            CometServer[Comet Server<br>(HTTP/WebSocket & gRPC)]
        end

        subgraph "Job 层 (job)"
            JobServer[Job Server<br>(Kafka Consumer & gRPC Client)]
        end

        subgraph "Logic 层 (logic)"
            LogicServer[Logic Server<br>(HTTP API)]
        end

        subgraph "中间件"
            Kafka[Kafka Message Queue]
        end

        subgraph "数据存储"
            MySQL[(MySQL DB<br>Users, Rooms, Messages)]
            Redis[(Redis Cache<br>Sessions, Message History)]
        end
    end

    Client -- "HTTP/WebSocket<br>连接/收发消息" --> CometServer
    Client -- "HTTP API<br>登录/注册/发消息/历史" --> LogicServer

    LogicServer -- "写消息" --> Kafka
    LogicServer -- "读/写" --> MySQL
    LogicServer -- "读/写" --> Redis

    JobServer -- "消费消息" --> Kafka
    JobServer -- "gRPC Push" --> CometServer

    CometServer -- "推送消息" --> Client
```

### 3.2 各层详细分析

#### Comet 层 (`chat-room`)
- **职责**: 维护与客户端的长连接（WebSocket），管理连接状态，并负责将消息实时推送给客户端。
- **核心组件**:
  - `HttpServer`: 基于Muduo库实现，处理HTTP和WebSocket连接请求。
  - `ConnectionManager`: 管理所有活跃的客户端连接。
  - `CometServiceImpl`: 实现gRPC服务，接收来自 `Job` 层的推送指令 (`PushMsg`, `Broadcast`, `BroadcastRoom`)。
  - `PubSubService`: 管理房间和用户的订阅关系，决定消息应该推送给哪些连接。
- **工作流程**:
  1. 客户端通过WebSocket连接到Comet服务器。
  2. Comet服务器在`ConnectionManager`中注册此连接。
  3. 当`Job`层通过gRPC调用`BroadcastRoom`时，`CometServiceImpl`接收请求。
  4. 它查询`PubSubService`，找到该房间下的所有活跃连接。
  5. 将消息通过对应的WebSocket连接推送给所有目标客户端。

#### Job 层 (`job`)
- **职责**: 作为一个后台作业处理器，消费来自Kafka的消息，并根据消息内容，通过gRPC将消息推送给一个或多个Comet服务器。
- **核心组件**:
  - `KafkaConsumer`: 连接到Kafka并订阅指定的主题（`my-topic`）。
  - `CometClient`: gRPC客户端，用于调用Comet层的gRPC服务。
  - `CometManager`: 管理与Comet服务器的gRPC连接。
- **工作流程**:
  1. `Job`服务启动后，`KafkaConsumer`开始从Kafka消费消息。
  2. 收到消息后，使用Protobuf解析为`PushMsg`结构。
  3. 根据`PushMsg`的`type`（`ROOM`或`BROADCAST`），决定调用哪个gRPC方法。
  4. 如果是`ROOM`消息，则调用`CometClient`的`broadcastRoom`方法，将消息内容和房间ID发送给Comet层。
  5. 如果是`BROADCAST`消息，则调用`broadcast`方法。

#### Logic 层 (`logic`)
- **职责**: 实现所有核心业务逻辑，是一个无状态的服务。
- **核心组件**:
  - `HttpServer`: 提供一组RESTful API，用于处理来自客户端的业务请求。
  - **API处理函数**: `handleLogin`, `handleRegister`, `handleSend`, `handleCreateRoom`等。
  - `KafkaProducer`: 用于将需要广播的消息生产到Kafka。
  - `MessageStorageManager`: 封装了对MySQL和Redis的访问，处理数据的持久化和缓存。
  - `RoomService`: 管理房间信息，如创建房间、获取房间列表等。
- **工作流程**:
  1. 接收来自客户端的HTTP请求（如发送消息）。
  2. 验证用户身份和请求参数。
  3. 将消息存入MySQL和Redis（`MessageStorageManager`）。
  4. 构建一个`PushMsg`（Protobuf）消息。
  5. 使用`KafkaProducer`将序列化后的`PushMsg`发送到Kafka。
  6. 向客户端返回HTTP响应，告知请求处理成功。

#### 数据库层 (MySQL & Redis)
- **MySQL**:
  - `user_infos`: 存储用户信息、密码哈希和盐。
  - `room_infos`: 存储房间信息。
  - `message_infos`: 存储所有聊天消息，作为持久化历史记录。
- **Redis**:
  - **Session缓存**: 存储用户登录后的`sid`（Session ID）与`userId`的映射，用于快速身份验证。
  - **消息缓存**: 缓存每个房间的最新N条消息，用于快速加载和减少数据库查询压力。

#### 中间件层 (Kafka)
- **作用**:
  - **解耦**: 将`Logic`层的业务处理与`Job`层的消息推送完全解耦。`Logic`层只需将消息丢入Kafka，无需关心哪些Comet服务器在线，也无需等待推送完成。
  - **削峰填谷**: 在高并发消息场景下，Kafka可以作为缓冲区，防止`Job`层和`Comet`层被瞬时流量冲垮。
  - **可扩展性**: 可以轻松地水平扩展`Job`层和`Comet`层的服务器数量，所有服务器都从Kafka消费或接收gRPC调用，实现了负载均衡和高可用。

## 第四部分：核心功能数据流

#### 1. 用户注册
`Client -> Logic -> MySQL`
1.  **Client**: 用户在前端页面填写用户名、邮箱、密码，点击注册。
2.  **Client**: 前端对密码进行哈希，然后将注册信息通过HTTP POST请求发送到 `Logic` 服务的 `/logic/register` 接口。
3.  **Logic**: 接收请求，检查用户名或邮箱是否已存在（查询MySQL）。
4.  **Logic**: 如果不存在，生成盐（salt），再次哈希密码，并将用户信息存入MySQL的 `user_infos` 表。
5.  **Logic**: 生成一个会话ID（`sid`），存入Redis，并将其通过`Set-Cookie`返回给客户端。

#### 2. 用户登录/认证
`Client -> Logic -> Redis/MySQL`
1.  **Client**: 用户输入邮箱和密码，前端哈希密码后发送到 `Logic` 服务的 `/logic/login` 接口。
2.  **Logic**: 查询MySQL验证用户信息和密码。
3.  **Logic**: 验证通过后，生成`sid`存入Redis，并通过`Set-Cookie`返回。
4.  **Client**: 后续请求（特别是WebSocket连接后的认证）会携带`sid`。`Logic`层的 `/logic/verify` 接口会查询Redis验证`sid`的有效性，实现快速认证。

#### 3. 发送聊天信息
`Client -> Logic -> Kafka -> Job -> Comet -> Other Clients`
1.  **Client**: 用户在聊天输入框输入消息，点击发送。
2.  **Client**: 将消息内容、房间ID、用户信息通过HTTP POST请求发送到 `Logic` 服务的 `/logic/send` 接口。
3.  **Logic**: 接收请求，验证用户身份。
4.  **Logic**: 将消息持久化到MySQL的 `message_infos` 表，并更新Redis中的房间最新消息缓存。
5.  **Logic**: 创建一个`PushMsg` Protobuf消息，`type`为`ROOM`，`operation`为`4`（代表聊天消息），`msg`字段包含完整的JSON消息体。
6.  **Logic**: 将序列化后的`PushMsg`消息推送到Kafka。
7.  **Job**: `Job`服务从Kafka消费到该消息。
8.  **Job**: 解析消息，得知需要向`room_123`房间广播。
9.  **Job**: 通过gRPC调用`Comet`服务的`BroadcastRoom`方法，传递房间ID和消息内容。
10. **Comet**: 接收gRPC请求，查找所有连接到`room_123`的客户端WebSocket连接。
11. **Comet**: 通过这些WebSocket连接，将消息推送到房间内的所有其他客户端。
12. **Other Clients**: 接收到WebSocket消息，在界面上渲染出新消息。

#### 4. 查询房间历史消息
`Client -> Logic -> Redis/MySQL`
1.  **Client**: 用户滚动到消息列表顶部，触发加载更多历史消息的事件。
2.  **Client**: 向 `Logic` 服务的 `/logic/room_history` 接口发送HTTP POST请求，包含房间ID、当前最早的消息ID和需要加载的数量。
3.  **Logic**: 接收请求，首先尝试从Redis缓存中获取历史消息。
4.  **Logic**: 如果Redis缓存不满足需求（例如，请求更早的消息），则查询MySQL的 `message_infos` 表，根据房间ID和时间戳进行分页查询。
5.  **Logic**: 将查询到的消息列表封装成JSON格式，通过HTTP响应返回给客户端。
6.  **Client**: 接收到历史消息列表，在消息列表的前端插入这些消息。

#### 5. 创建房间
`Client -> Logic -> MySQL & Kafka -> Job -> Comet -> All Clients`
1.  **Client**: 用户点击“创建房间”，输入房间名。
2.  **Client**: 将房间名和创建者信息通过HTTP POST请求发送到 `Logic` 服务的 `/logic/room/create` 接口。
3.  **Logic**: 接收请求，检查房间名是否已在MySQL中存在。
4.  **Logic**: 如果不存在，将新房间信息存入MySQL的 `room_infos` 表。
5.  **Logic**: 向请求方客户端返回创建成功的HTTP响应，包含新房间的ID和名称。
6.  **Logic**: 同时，创建一个`PushMsg` Protobuf消息，`type`为`BROADCAST`，`operation`为`5`（代表创建房间），`msg`字段包含新房间信息的JSON。
7.  **Logic**: 将此消息推送到Kafka。
8.  **Job**: `Job`服务消费到此广播消息。
9.  **Job**: 通过gRPC调用`Comet`服务的`Broadcast`方法。
10. **Comet**: 接收请求，将新房间创建的消息推送给所有在线的客户端。
11. **All Clients**: 接收到WebSocket广播，动态地在房间列表中添加这个新房间。