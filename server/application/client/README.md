# 聊天室客户端

这是一个基于 WebSocket 的聊天室客户端程序。

## 编译

和其他服务端一样，使用CMake编译，然后运行。

## 运行
需要在web客户端提前注册账号，然后使用账号登录。

### 基本用法

```bash
./bin/client                         # 使用默认账号登录(10@qq.com/1234)
./bin/client email password         # 使用指定的邮箱和密码登录
```

例如:
```bash
./bin/client test@example.com 123456
```

### 聊天命令

连接成功后，支持以下命令:

1. `room:房间ID` - 切换到指定的聊天室
   ```
   room:12345
   ```

2. `create:房间名称` - 创建新的聊天室
   ```
   create:我的聊天室
   ```

3. `history:房间ID` - 获取指定房间的历史消息
   ```
   history:12345
   ```

4. `history:房间ID:消息ID:数量` - 获取指定消息之前的历史消息
   ```
   history:12345:msg_id_xxx:20
   ```

5. `quit` - 退出程序

### 发送消息

切换到指定聊天室后，直接输入文本内容即可发送消息。

## 注意事项

1. 首次使用需要先登录
2. 发送消息前需要先使用 `room:房间ID` 命令选择一个聊天室
3. 如果没有可用的聊天室，可以使用 `create:房间名称` 命令创建新的聊天室

## 配置说明

默认配置:
- 服务器地址: 127.0.0.1
- 服务器端口: 8080
- 默认用户: 10@qq.com
- 默认密码: 1234

## 示例会话

```bash
$ ./bin/client test@example.com 123456
[INFO] 连接成功，开始聊天
[INFO] 支持的命令：
1. room:房间ID          - 切换房间
2. create:房间名称      - 创建新房间
3. history:房间ID       - 获取房间历史消息
4. history:房间ID:消息ID:数量 - 获取指定消息之前的历史消息
5. quit                 - 退出程序
直接输入文本发送消息

> create:测试房间
[INFO] 新房间创建 - ID: 12345, 名称: 测试房间

> room:12345
[INFO] 切换到房间: 12345

> Hello, everyone!
[INFO] 消息已发送

> history:12345
[INFO] 房间历史消息...

> quit
```

 
