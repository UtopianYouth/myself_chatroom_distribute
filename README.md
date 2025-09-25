# 分布式聊天室

## 一、项目部署

**客户端**

```bash
cd client/web4
npm install
npm run dev
```

**服务端**

```bash
cd server
mkdir build
cd build
# single
cmake -DCMAKE_BUILD_TYPE=Debug ..
# grpc
cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_RPC=ON ..
make
```



## 二、项目技术栈选型

c++11 & 单例模式 & muduo 网络库 & MySQL & redis & RPC & Kafka

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

### 3.1 各个服务功能

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
> - 用户认证，会话管理（未实现）；
>
> - 消息合法性校验、内容处理（先不用实现）；
> - 调用存储层进行数据持久化或查询（已实现）；
>
> - 将需要异步处理或广播的消息发布到 Kafka 中（已实现）。
>
> **job 层**：由多个job节点组成，负责消息的消费与推送，职责如下：
>
> - 从 Kafka 消费消息（如群聊、私聊、系统通知）；
> - 查询用户在线状态和连接信息（哪个comet节点）；
> - 通过 gRPC 调用对应的 comet 节点，将消息实时推送给客户端；
> - 支持离线消息补偿，可结合MySQL实现（先不用实现）。

## 四、数据流向详解

### 4.1 单机下的数据流

```apl
前端请求流程：
┌─────────────────┐    Next.js Rewrite    ┌──────────────────┐
│ fetch('/api/login') │ ──────────────────→ │ http://localhost:8080/api/login │
└─────────────────┘                        └──────────────────┘
                                                    │
后端处理流程：                                        ▼
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   TcpServer     │───→│   HttpHandler   │───→│   CHttpConn     │
│  (port 8080)    │    │  (请求类型判断)   │    │  (URL路径匹配)   │
└─────────────────┘    └─────────────────┘    └─────────────────┘
                                                    │
                                                    ▼
                                          ┌─────────────────┐
                                          │  API处理函数     │
                                          │ ApiLoginUser()  │
                                          └─────────────────┘

```

### 4.2分布式下的数据流

**消息发送流程**

```
用户发送消息 → Logic服务接收HTTP请求 → 解析并序列化为PushMsg → 
发送到Kafka → Job服务消费消息 → 通过gRPC调用ChatRoom → 
ChatRoom广播到WebSocket连接
```

**关键数据结构**

> PushMsg (Kafka消息格式)
>
> ```c++
> message PushMsg {
>     Type type = 1;        // ROOM=房间消息, BROADCAST=广播, PUSH=点对点
>     int32 operation = 2;  // 4=发送消息
>     string room = 5;      // 房间ID
>     bytes msg = 7;        // JSON格式的消息内容
> }
> ```
>
> gRPC 协议格式
>
> ```c++
> message Proto {
>     int32 ver = 1;    // 协议版本
>     int32 op = 2;     // 操作码
>     int32 seq = 3;    // 序列号
>     bytes body = 4;   // 消息体
> }
> ```
>
> 

## 五、服务端接口协议总结

| 服务               | 端口  | 协议           | 作用               |
| :----------------- | :---- | :------------- | :----------------- |
| Logic              | 8090  | HTTP           | 接收客户端消息请求 |
| Kafka              | 9092  | Kafka Protocol | 消息队列中转       |
| Job                | -     | Kafka Consumer | 消费消息并转发     |
| ChatRoom gRPC      | 50051 | gRPC           | 接收Job的广播请求  |
| ChatRoom WebSocket | 8080  | WebSocket      | 向客户端推送消息   |

## 六、优化

随着项目功能的不断拓展，优化的思路也会逐渐清晰。

### 6.1 存储层优化

MySQL作为消息的持久化存储是否合理？



## 七、一些神奇BUG记录

**批量插入聊天记录到数据库的变量生命周期问题：**`SetParam()` 第二个值接收的是引用，不能在循环内部定义拷贝变量，不然在`ExecuteUpdate()`执行时，变量生命周期结束，导致SQL执行异常。

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

**redis中不能存储特殊字符的聊天信息，譬如空格等：**修改了 `cache_pool.cc` 基本框架的源码。



### 八、项目中一些值得深思的问题

在做项目的各个模块时，应该优先考虑先把功能做出来，还是在做某一个功能模块的时候，优先考虑性能，这涉及到项目上线的效率问题？

关于Kafka在项目中的真正优势，comet层对请求进行转发到logic层后，为什么不直接接收logic层的响应数据？而是logic层将消息发送到Kafka中，再由job层消费Kafka中的数据，通过gRPC调用comet层，对响应消息具体处理的业务逻辑。
