# 分布式聊天室

## 一、项目部署

**环境依赖**

> 前端
>
> - node-v21.7.3-linux-x64
>
> 后端
>
> - Ubuntu20.04
>- MySQL 8.0.42
> - Redis 6.0.16
>- gcc/g++ 10.5.0
> - kafka_2.13-3.7.2
>- gRPC/libprotoc 3.19.4
> - kafka 2.13-3.7.2

**前端**

```bash
cd client/web
npm install
npm run dev
```

**后端**

```bash
cd server
mkdir build
cd build
# single
cmake -DCMAKE_BUILD_TYPE=Debug ..
# grpc
cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_RPC=ON ..
make clean
make 
```

**分布式下后端启动流程**

```bash
cd server/build
# 1. 第一步启动 logic 层
./bin/logic

# 2.第二步启动 comet 层，该层需要调用 logic 层的接口，操作数据库
./bin/chat-room

# 3. 第三步启动 job 层，kafka消费者，也是主要的grpc发起层
./bin/job
```

## 二、项目技术栈选型

c++17 & 单例模式 & muduo 网络库 & MySQL & redis & RPC & Kafka

## 三、分布式架构概览

分布式聊天室采用微服务架构，包括以下核心组件，概览图大致如下：

```apl
┌─────────────┐    ┌─────────────┐    ┌─────────────┐    ┌─────────────┐
│   客户端     │    │  Logic服务   │    │    Kafka    │    │  Job服务     │
│  (Web/App)  │───▶│   (8090)    │───▶│  消息队列    │───▶│  (消费者)    │
└─────────────┘    └─────────────┘    └─────────────┘    └─────────────┘
                                                                │
                                                                ▼
┌─────────────┐    ┌─────────────┐                    ┌─────────────┐
│  WebSocket  │◀───│ ChatRoom服务 │◀──────────────────│   gRPC调用   │
│   客户端     │    │   (8080)    │                    │             │
└─────────────┘    └─────────────┘                    └─────────────┘
```

### 3.1 各层职能

> **comet层**，由多个 Comet 节点组成，每个节点是websocket的网关服务，职责如下：
>
> - 接收并维持客户端的 websocket 长连接；
> - 将客户端消息转发给后端 logic，接收job的推送消息，执行对应的业务逻辑，并且将响应结果通过websocket连接实时推送给客户端；
> - 支持水平扩展，应对海量 websocket 连接。
>
> - 通信协议，与客户端使用 websocket 连接通信，与逻辑层使用 gRPC。
>
> **logic层**，由多个 logic 服务节点组成，处理核心业务逻辑，包含一个负载均衡器，用于将HTTP请求均分发到 logic 节点，职责如下：
>
> - 用户认证，会话管理；
>
> - 消息合法性校验、内容处理；
> - 调用存储层进行数据持久化或查询；
>
> - 将需要异步处理或广播的消息发布到 Kafka 中。
>
> **job 层**：由多个job节点组成，负责消息的消费与推送，职责如下：
>
> - 从 Kafka 消费消息（如群聊、私聊、系统通知）；
> - 查询用户在线状态和连接信息（哪个comet节点）；
> - 通过 gRPC 调用对应的 comet 节点，将消息实时推送给客户端；
> - 支持离线消息补偿，可结合MySQL实现（先不用实现）。

## 四、数据流向详解

### 4.3 websocket帧格式

帧格式涉及到解析HTTP请求和生成HTTP响应的逻辑，这里一定要弄清楚。

#### 4.3.1 发送消息

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

#### 4.3.2 获取房间历史消息

**客户端消息格式：**

```json
{
  "type": "requestRoomHistory",
  "payload": {
    "roomId": "room_id",
    "firstMessageId": "message_id", 
    "count": 10
  }
}
```

**服务器响应格式：**

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

#### 4.3.3 创建房间

**客户端消息格式：**

```json
{
  "type": "clientCreateRoom",
  "payload": {
    "roomName": "房间名称"
  }
}
```

**服务器响应格式：**



#### 4.3.4 hello 格式

用户登录后发送 hello 帧，获取聊天室的所有房间信息和历史消息。

**客户端消息格式：**

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

**服务器响应格式：**



## 五、服务端接口协议总结

| 服务               | 端口  | 协议           | 作用               |
| :----------------- | :---- | :------------- | :----------------- |
| Logic              | 8090  | HTTP           | 接收客户端消息请求 |
| Kafka              | 9092  | Kafka Protocol | 消息队列中转       |
| Job                | -     | Kafka Consumer | 消费消息并转发     |
| ChatRoom gRPC      | 50051 | gRPC           | 接收Job的广播请求  |
| ChatRoom WebSocket | 8080  | WebSocket      | 向客户端推送消息   |

## 六、优化

项目最开始是单机版的，在不断了解项目的过程中，不断优化前端和后端，使得项目可玩性增加。

### 6.1 前端

> - 第一部分主要是登录、注册和聊天界面的优化显示，使得前端页面看起来更加现代化一点，包括页面布局，按钮无边框设计等；
>
> - 第二部分主要是对聊天界面的细节优化
>   - 增加聊天室分类展示，包括三类：所有房间、用户创建的房间和其它人创建的房间，不过多人聊天，大部分时间都在所有房间下；
>   - 聊天框的优化，包括显示当前用户的头像，显示当前聊天房间的名字，增加了查找和获取30天之前（需要操作MySQL）历史消息的按钮（后端业务逻辑还没有实现，后续拓展很方便）；
>   - 输入消息框和输入按钮的优化；
> - 第三部分主要是对网页顶部的优化，包括布局等，参考主流互联网公司的主页设计。

### 6.2 后端

后端层的优化，大部分的工作是在服务迁移上面，单机版的聊天室，所有的业务逻辑都在 comet 层，显然是不合理的，所以需要将其迁移到 logic 中去，comet 层只需要提供访问 logic 层的请求转发业务逻辑即可。

#### 6.2.1 comet 层



#### 6.2.2 job 层



#### 6.2.3 logic 层



#### 6.2.4 存储层

在业务的不断迁移过程中，存储层的优化主要工作是两部分，表结构及其字段的优化和新增表 CRUD 业务的完善，在核心业务迁移的过程中，也思考了一些问题。



MySQL作为消息的持久化存储是否合理？

合理，针对前30天历史消息，使用 redis 存储，也就是用户通过上拉获取历史消息的上限是30天前；30天之后的历史消息使用 MySQL 存储。



哪些数据应该一次加载之后，常驻内存？



## 七、一些神奇BUG记录

**批量插入聊天记录到数据库时，变量生命周期问题**

`SetParam()` 第二个值接收的是引用，不能在循环内部定义拷贝变量，不然在`ExecuteUpdate()`执行时，变量生命周期结束，导致SQL执行异常。

```cpp
/*
	代码定位：/server/application/logic/api/api_message.cc ==> storeMessagesToDB
	
*/

std::vector<string> msg_ids, room_ids;
std::vector<uint32_t> user_ids;

// 预先分配空间
msg_ids.reserve(messages.size());
room_ids.reserve(messages.size());
user_ids.reserve(messages.size());

for (const auto& msg : messages) {
  msg_ids.emplace_back(msg.id.empty() ? generateMessageId() : msg.id);
  room_ids.emplace_back(room_id);  // 来自函数参数，需要为每条消息复制
  user_ids.emplace_back(static_cast<uint32_t>(msg.user_id));  // 需要类型转换
}

int param_index = 0;
for (size_t i = 0; i < messages.size(); ++i) {
  stmt.SetParam(param_index++, msg_ids[i]);
  stmt.SetParam(param_index++, room_ids[i]);
  stmt.SetParam(param_index++, user_ids[i]);
  stmt.SetParam(param_index++, messages[i].username);
  stmt.SetParam(param_index++, messages[i].content);
}

if (!stmt.ExecuteUpdate()) {
  LOG_ERROR << "Failed to execute batch SQL statement";
  return false;
}
```

**redis中不能存储特殊字符的聊天信息，譬如空格等**

修改了 `cache_pool.cc` 源码，将redis命令从简单拼接的形式改为了调用`redisCommandArgv()`组合redis命令的形式，后者只需要提供插入的键值对即可，避免了拼接造成的特殊字符无法识别问题。



## 八、项目中一些值得深思的问题

在做项目的各个模块时，应该优先考虑先把功能做出来，还是在做某一个功能模块的时候，优先考虑性能，这涉及到项目上线的效率问题？

关于Kafka在项目中的真正优势，comet层对请求进行转发到logic层后，为什么不直接接收logic层的响应数据？而是logic层将消息发送到Kafka中，再由job层消费Kafka中的数据，通过gRPC调用comet层，对响应消息具体处理的业务逻辑。

什么是单例模式，在项目中频繁使用单例模式代替函数式接口有什么风险？

项目核心功能目前有三大块，处理用户发送的聊天信息（使用Kafka的消息推送形式），获取房间历史消息（不属于消息推送原则，请求转发直接响应模式），创建房间（不属于消息推送原则，请求转发直接响应模式）。==> 深入思考为什么要引入Kafka和gRPC，而不是直接响应的形式。



分布式下，创建房间的广播消息具体是怎样实现的。



设置操作码，不用解析json数据来验证。



前端优化大致路径：

> UI 的优化，我的房间和其它房间分开显示；
>
> 一些布局调整；
>
> 新增所有房间显示；
>
> 所有房间、我的房间、其它房间
