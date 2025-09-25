# Logic服务目录结构说明

## 整理后的目录结构

```
logic/
├── main.cc                    # 主程序入口
├── logic.conf                 # 配置文件
├── CMakeLists.txt            # 构建配置
├── api/                      # API相关模块
│   ├── api_common.h/cc       # 通用API工具函数
│   ├── api_types.h           # API数据结构定义
│   ├── message_storage.h/cc  # 消息存储服务
│   └── message_api.h/cc      # 消息查询API
├── service/                  # 业务服务模块
│   ├── kafka_producer.h/cpp  # Kafka生产者服务
│   └── http_parser.h         # HTTP请求解析工具
├── proto/                    # Protocol Buffers定义
│   ├── ChatRoom.Job.proto    # 作业消息协议定义
│   ├── ChatRoom.Job.pb.h     # 生成的protobuf头文件
│   └── ChatRoom.Job.pb.cc    # 生成的protobuf实现
├── mysql/                    # MySQL数据库模块
│   ├── db_pool.h/cc          # 数据库连接池
├── redis/                    # Redis缓存模块
│   ├── cache_pool.h/cc       # Redis连接池
│   └── hiredis相关文件       # Redis客户端库
├── base/                     # 基础工具模块
│   ├── config_file_reader.h/cc # 配置文件读取
│   ├── util.h/cc             # 通用工具函数
│   └── base64.h/cpp          # Base64编解码
├── doc/                      # 文档目录
│   ├── API_DOCUMENTATION.md  # API接口文档
│   ├── README.md             # 项目说明
│   └── DIRECTORY_STRUCTURE.md # 目录结构说明
└── scripts/                  # 脚本目录
    └── create.sh             # 创建脚本
```

## 模块功能说明

### API模块 (api/)
- **api_common**: 提供通用的API工具函数，如UUID生成、Cookie管理等
- **api_types**: 定义消息、用户等数据结构
- **message_storage**: 消息持久化存储服务，支持MySQL和Redis双写
- **message_api**: 消息查询API，提供历史消息检索功能

### 服务模块 (service/)
- **kafka_producer**: Kafka消息生产者，负责将消息发送到消息队列
- **http_parser**: HTTP请求解析工具，处理REST API请求

### 协议模块 (proto/)
- 包含Protocol Buffers定义和生成的代码
- 定义了与Job服务通信的消息格式

### 数据存储模块
- **mysql/**: MySQL数据库连接和操作
- **redis/**: Redis缓存连接和操作

### 基础模块 (base/)
- 提供配置文件读取、通用工具函数等基础功能

## 编译说明

使用server/build目录进行编译：
```bash
cd server/build
make logic
```

编译成功后，可执行文件位于：`server/bin/logic`

## 主要功能

1. **消息发送**: POST /logic/send - 接收消息并发送到Kafka队列，同时存储到数据库
2. **历史查询**: GET /logic/history - 查询房间历史消息
3. **用户登录**: POST /logic/login - 简单的登录处理

## 配置文件

使用 `logic.conf` 配置数据库连接、Redis连接等参数。