## 七、优化过程

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



## 八、一些神奇BUG记录

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



## 九、项目中一些值得深思的问题

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



docker镜像遇到的一些问题

第三方依赖从本地安装还是git clone的形式

需要的中间件也要单独做成镜像

一旦镜像生成，镜像中包含了第三方依赖，那么每次启动容器都需要重新安装依赖，如何解决？



在制作docker镜像的过程中，前端项目集成了nginx服务，但是前端项目的资源都是动态资源，不合理，需要拆分。



关于logic层，可以将各个服务进行进一步拆分。



base库是从哪里来的，读取配置文件，工具库等，从github开源的吗？



服务端的 Dockerfile 将comet层，logic层和job层构建成了一个docker镜像。



项目拆分：重建CMakeList，出现muduo网络库重复引用问题。



问题：前端开放80端口，websocket连接的处理逻辑需要清晰，不然项目部署会有问题。



查询历史消息的bug问题。