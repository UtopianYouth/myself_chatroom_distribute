#include "api_message.h"

int ApiGetRoomHistory(Room& room, MessageBatch& message_batch) {
    CacheManager* cache_manager = CacheManager::getInstance();
    CacheConn* cache_conn = cache_manager->GetCacheConn("msg");
    AUTO_REL_CACHECONN(cache_manager, cache_conn);

    string stream_ref = "+";
    if (!room.history_last_message_id.empty()) {
        stream_ref = "(" + room.history_last_message_id;
    }
    LOG_INFO << "stream_ref: " << stream_ref;
    std::vector<pair<string, string>> msgs;

    // get data from redis
    if (cache_conn->GetXrevrange(room.room_id, stream_ref, "-", message_batch_size, msgs)) {
        for (int i = 0;i < msgs.size();++i) {
            LOG_INFO << "key: " << msgs[i].first << ", value: " << msgs[i].second;
            Message msg;
            msg.id = msgs[i].first;     // message id
            room.history_last_message_id = msg.id;      // last message id
            bool res;
            Json::Value root;
            Json::Reader jsonReader;
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

            if (root["timestamp"].isNull()) {
                LOG_ERROR << "timestamp null";
                return -1;
            }
            msg.timestamp = root["timestamp"].asInt64();
            message_batch.messages.push_back(msg);
        }

        if (msgs.size() < message_batch_size) {
            message_batch.has_more = false;     // redis haven't more data can be read
        }
        else {
            message_batch.has_more = true;
        }

        return 0;
    }
    else {
        return -1;
    }
}

// Message to json
string SerializeMessageToJson(const std::vector<Message>& msgs) {
    Json::Value root;
    Json::StreamWriterBuilder writer;

    for (const auto& msg : msgs) {
        Json::Value msgNode;
        msgNode["id"] = msg.id;
        msgNode["content"] = msg.content;
        msgNode["timestamp"] = (Json::UInt64)msg.timestamp;
        msgNode["user_id"] = (Json::Int64)msg.user_id;

        root.append(msgNode);
    }

    // json tree converts string
    return Json::writeString(writer, root);
}

string SerializeMessageToJson(const Message& msg) {
    Json::Value root;
    Json::StreamWriterBuilder writer;
    writer.settings_["indentation"] = "";   // forbid '\n' and '\r'

    root["content"] = msg.content;
    root["timestamp"] = (Json::Int64)msg.timestamp;
    root["user_id"] = (Json::Int64)msg.user_id;

    // json tree converts string
    return Json::writeString(writer, root);
}

// store data into redis
int ApiStoreMessage(string room_id, std::vector<Message>& msgs) {
    CacheManager* cache_manager = CacheManager::getInstance();
    CacheConn* cache_conn = cache_manager->GetCacheConn("msg");
    AUTO_REL_CACHECONN(cache_manager, cache_conn);

    for (int i = 0;i < msgs.size();++i) {
        string json_msg = SerializeMessageToJson(msgs[i]);
        std::vector<pair<string, string>> field_value_pairs;
        LOG_INFO << "payload: " << json_msg;
        field_value_pairs.push_back({ "payload", json_msg });
        string id = "*";
        bool  ret = cache_conn->Xadd(room_id, id, field_value_pairs);
        if (!ret) {
            LOG_ERROR << "ApiStoreMessage room_id: " << room_id << " failed";
            return -1;
        }
        LOG_INFO << "msgs id: " << id;
        msgs[i].id = id;
    }

    return 0;
}

