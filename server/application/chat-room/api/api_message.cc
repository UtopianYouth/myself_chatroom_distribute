#include "api_message.h"

int ApiGetRoomHistory(Room& room, MessageBatch& message_batch, const int msg_count) {
    CacheManager* cache_manager = CacheManager::getInstance();
    CacheConn* cache_conn = cache_manager->GetCacheConn("msg");
    AUTO_REL_CACHECONN(cache_manager, cache_conn);

    std::string stream_ref = "+";       // hello message ==> get history message firstly
    if (!room.history_last_message_id.empty()) {
        stream_ref = "(" + room.history_last_message_id;
    }
    LOG_DEBUG << "stream_ref: " << stream_ref << ", msg_count:" << msg_count << ", room_id:" << room.room_id;

    std::vector<std::pair<string, string>> msgs;
    bool res = cache_conn->GetXrevrange(room.room_id, stream_ref, "-", msg_count, msgs);

    // get data from redis
    if (res) {
        for (int i = 0;i < msgs.size();++i) {
            // the first message has been display
# if 1
            Message msg;
            msg.id = msgs[i].first;                     // message id
            room.history_last_message_id = msg.id;      // last message id
            bool res;
            Json::Value root;
            Json::Reader jsonReader;
            LOG_DEBUG << "msg: " << msgs[i].second;
            res = jsonReader.parse(msgs[i].second, root);
            if (!res) {
                LOG_ERROR << "parse redis msg failed";
                return -1;
            }

            if (root["content"].isNull()) {
                LOG_ERROR << "content null";
                return -1;
            }
            msg.content = root["content"].asString();

            if (root["user_id"].isNull()) {
                LOG_ERROR << "user_id null";
                return -1;
            }
            msg.user_id = root["user_id"].asInt64();

            if (root["username"].isNull()) {
                LOG_ERROR << "username null";
                return -1;
            }
            msg.username = root["username"].asString();

            if (root["timestamp"].isNull()) {
                LOG_ERROR << "timestamp null";
                return -1;
            }
            msg.timestamp = root["timestamp"].asInt64();
#endif 

            message_batch.messages.push_back(msg);
        }

        LOG_DEBUG << "room_id: " << room.room_id << " , msgs.size(): " << msgs.size();

        if (msgs.size() < msg_count) {
            message_batch.has_more = false;     // redis haven't more data can be read
        }
        else {
            message_batch.has_more = true;      // after this get message batch from redis, has more message in redis 
        }

        return 0;
    }
    else {
        return -1;
    }
}

// store data into redis
int ApiStoreMessage(string room_id, std::vector<Message>& msgs) {
    CacheManager* cache_manager = CacheManager::getInstance();
    CacheConn* cache_conn = cache_manager->GetCacheConn("msg");
    AUTO_REL_CACHECONN(cache_manager, cache_conn);

    //LOG_INFO << "msgs.size(): " << msgs.size();

    for (int i = 0;i < msgs.size();++i) {
        string json_msg = SerializeMessageToJson(msgs[i]);
        std::vector<pair<string, string>> field_value_pairs;

        // redis 6.0.16: json data need enclosed in single quotation marks
        //json_msg = "'" + json_msg + "'";
        field_value_pairs.push_back({ "payload", json_msg });

        LOG_DEBUG << "room_id: " << room_id << ", payload: " << json_msg;

        string id = "*";
        bool  ret = cache_conn->Xadd(room_id, id, field_value_pairs);

        if (!ret) {
            LOG_ERROR << "Xadd failed";
            return -1;
        }

        //LOG_INFO << "room_id:" << room_id << ", msg.id:" << id << ", json_msg:" << json_msg;
        msgs[i].id = id;
    }
    return 0;
}

// serialize message to json string
string SerializeMessageToJson(const Message& msg) {
    Json::Value root;  // 创建JSON::Value对象，用于构建JSON数据结构
    // 添加消息内容到JSON根节点
    root["content"] = msg.content;
    // 添加时间戳到JSON根节点，并转换为Json::UInt64类型
    root["timestamp"] = (Json::UInt64)msg.timestamp;
    root["user_id"] = (Json::UInt64)msg.user_id;
    root["username"] = msg.username;
    Json::FastWriter writer;

    return writer.write(root);
    //return root.toStyledString();
}


