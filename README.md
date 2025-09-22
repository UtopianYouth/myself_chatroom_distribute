# 分布式聊天室

## 项目部署

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



## 项目技术栈选型

c++11 & 单例模式 & muduo 网络库 & MySQL & redis & RPC & Kafka

## 架构概览

分布式聊天室采用微服务架构，包括以下核心组件：

```
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

## 数据流向详解

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



