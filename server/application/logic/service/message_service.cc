#include "message_service.h"
#include "../api/api_common.h"
#include "../base/config_file_reader.h"
#include <chrono>
#include <sstream>
#include <iomanip>
#include <numeric>
#include <json/json.h>
#include <muduo/base/Logging.h>

MessageStorageManager& MessageStorageManager::getInstance() {
    static MessageStorageManager instance;
    return instance;
}

bool MessageStorageManager::init(const string& config_file) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (initialized_) {
        return true;
    }

    try {
        config_file_ = config_file;

        // 初始化数据库连接池
        CDBManager::SetConfPath(config_file.c_str());
        CDBManager* db_manager = CDBManager::getInstance();
        if (!db_manager) {
            LOG_ERROR << "Failed to initialize database manager";
            return false;
        }

        // 初始化Redis连接池
        CacheManager::SetConfPath(config_file.c_str());
        CacheManager* cache_manager = CacheManager::getInstance();
        if (!cache_manager) {
            LOG_ERROR << "Failed to initialize cache manager";
            return false;
        }

        initialized_ = true;
        LOG_INFO << "MessageStorageManager initialized successfully";
        return true;

    }
    catch (const std::exception& e) {
        LOG_ERROR << "Failed to initialize MessageStorageManager: " << e.what();
        return false;
    }
}

string MessageStorageManager::generateMessageId() {
    // 时间戳 + UUID
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

    string uuid_str = GenerateUUID();

    return std::to_string(timestamp) + "-" + uuid_str;
}

string MessageStorageManager::serializeMessageToJson(const Message& msg) {
    Json::Value root;
    root["content"] = msg.content;
    root["timestamp"] = (Json::UInt64)msg.timestamp;
    root["user_id"] = msg.user_id;
    root["username"] = msg.username;

    Json::FastWriter writer;
    return writer.write(root);
}

CDBConn* MessageStorageManager::getDBConnection() {
    CDBManager* db_manager = CDBManager::getInstance();
    return db_manager ? db_manager->GetDBConn("chatroom_master") : nullptr;
}

CacheConn* MessageStorageManager::getCacheConnection() {
    CacheManager* cache_manager = CacheManager::getInstance();
    return cache_manager ? cache_manager->GetCacheConn("msg") : nullptr;
}

void MessageStorageManager::releaseDBConnection(CDBConn* conn) {
    if (conn) {
        CDBManager* db_manager = CDBManager::getInstance();
        if (db_manager) {
            db_manager->RelDBConn(conn);
        }
    }
}

void MessageStorageManager::releaseCacheConnection(CacheConn* conn) {
    if (conn) {
        CacheManager* cache_manager = CacheManager::getInstance();
        if (cache_manager) {
            cache_manager->RelCacheConn(conn);
        }
    }
}

int MessageStorageManager::getRoomHistory(Room& room, MessageBatch& message_batch, const int msg_count) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        LOG_ERROR << "MessageStorageManager not initialized";
        return -1;
    }

    CacheConn* cache_conn = getCacheConnection();
    if (!cache_conn) {
        LOG_ERROR << "Failed to get cache connection";
        return -1;
    }

    // 使用RAII自动释放连接
    auto conn_guard = [this](CacheConn* conn) { releaseCacheConnection(conn); };
    std::unique_ptr<CacheConn, decltype(conn_guard)> conn_ptr(cache_conn, conn_guard);

    std::string stream_ref = "+";
    if (!room.history_last_message_id.empty()) {
        stream_ref = room.history_last_message_id;  // 不使用开区间语法
    }

    LOG_DEBUG << "stream_ref: " << stream_ref << ", msg_count:" << msg_count << ", room_id:" << room.room_id;

    std::vector<std::pair<string, string>> msgs;
    bool res = cache_conn->GetXrevrange(room.room_id, stream_ref, "-", msg_count, msgs);

    if (res) {
        // "+"从第一条开始
        int index = 0;
        if (!room.history_last_message_id.empty()) {
            ++index;  // 跳过第一条消息
        }

        for (;index < msgs.size();++index) {
            Message msg;
            msg.id = msgs[index].first;
            room.history_last_message_id = msg.id;

            Json::Value root;
            Json::Reader jsonReader;
            LOG_DEBUG << "msg: " << msgs[index].second;

            if (!jsonReader.parse(msgs[index].second, root)) {
                LOG_ERROR << "parse redis msg failed";
                return -1;
            }

            if (root["content"].isNull() || root["user_id"].isNull() ||
                root["username"].isNull() || root["timestamp"].isNull()) {
                LOG_ERROR << "Missing required fields in message";
                return -1;
            }

            msg.content = root["content"].asString();
            msg.user_id = root["user_id"].asString();
            msg.username = root["username"].asString();
            msg.timestamp = root["timestamp"].asInt64();

            message_batch.messages.push_back(msg);
        }

        LOG_INFO << "room_id: " << room.room_id << " , msgs.size(): " << msgs.size();
        message_batch.has_more = (msgs.size() >= msg_count);
        return 0;
    }

    return -1;
}

bool MessageStorageManager::storeMessageToCache(const string& room_id, const Message& msg) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        LOG_ERROR << "MessageStorageManager not initialized";
        return false;
    }

    try {
        CacheConn* cache_conn = getCacheConnection();
        if (!cache_conn) {
            LOG_ERROR << "Failed to get cache connection";
            return false;
        }

        // 使用RAII自动释放连接
        auto conn_guard = [this](CacheConn* conn) { releaseCacheConnection(conn); };
        std::unique_ptr<CacheConn, decltype(conn_guard)> conn_ptr(cache_conn, conn_guard);

        string json_msg = serializeMessageToJson(msg);

        std::vector<std::pair<string, string>> field_value_pairs;
        field_value_pairs.push_back({ "payload", json_msg });

        string id = "*";
        bool ret = cache_conn->Xadd(room_id, id, field_value_pairs);

        if (!ret) {
            LOG_ERROR << "Failed to store message to Redis";
            return false;
        }

        LOG_INFO << "Message stored to cache successfully: " << id;
        return true;

    }
    catch (const std::exception& e) {
        LOG_ERROR << "Error storing message to cache: " << e.what();
        return false;
    }
}

int MessageStorageManager::storeMessagesToCache(const string& room_id, std::vector<Message>& msgs) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        LOG_ERROR << "MessageStorageManager not initialized";
        return -1;
    }

    CacheConn* cache_conn = getCacheConnection();
    if (!cache_conn) {
        LOG_ERROR << "Failed to get cache connection";
        return -1;
    }

    // 使用RAII自动释放连接
    auto conn_guard = [this](CacheConn* conn) { releaseCacheConnection(conn); };
    std::unique_ptr<CacheConn, decltype(conn_guard)> conn_ptr(cache_conn, conn_guard);

    for (auto& msg : msgs) {
        string json_msg = serializeMessageToJson(msg);
        std::vector<pair<string, string>> field_value_pairs;
        field_value_pairs.push_back({ "payload", json_msg });

        LOG_DEBUG << "room_id: " << room_id << ", payload: " << json_msg;

        string id = "*";
        bool ret = cache_conn->Xadd(room_id, id, field_value_pairs);

        if (!ret) {
            LOG_ERROR << "Xadd failed";
            return -1;
        }

        msg.id = id;    // redis generates the id
    }

    LOG_INFO << "Stored " << msgs.size() << " messages to cache successfully";
    return 0;
}

bool MessageStorageManager::storeMessageToDB(const string& room_id, const Message& msg) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        LOG_ERROR << "MessageStorageManager not initialized";
        return false;
    }

    try {
        CDBConn* db_conn = getDBConnection();
        if (!db_conn) {
            LOG_ERROR << "Failed to get database connection";
            return false;
        }

        // 使用RAII自动释放连接
        auto conn_guard = [this](CDBConn* conn) { releaseDBConnection(conn); };
        std::unique_ptr<CDBConn, decltype(conn_guard)> conn_ptr(db_conn, conn_guard);

        string sql = "INSERT INTO message_infos "
            "(`msg_id`, `room_id`, `user_id`, `username`, `msg_content`) "
            "VALUES (?, ?, ?, ?, ?)";
        CPrepareStatement stmt;
        if (!stmt.Init(db_conn->GetMysql(), sql)) {
            LOG_ERROR << "Failed to prepare SQL statement";
            return false;
        }

        string msg_id = msg.id.empty() ? generateMessageId() : msg.id;

        stmt.SetParam(0, msg_id);
        stmt.SetParam(1, room_id);
        stmt.SetParam(2, msg.user_id);
        stmt.SetParam(3, msg.username);
        stmt.SetParam(4, msg.content);

        if (!stmt.ExecuteUpdate()) {
            LOG_ERROR << "Failed to execute SQL statement";
            return false;
        }

        LOG_INFO << "Message stored to database successfully: " << msg_id;
        return true;

    }
    catch (const std::exception& e) {
        LOG_ERROR << "Error storing message to database: " << e.what();
        return false;
    }
}

bool MessageStorageManager::storeMessagesToDB(const string& room_id, const std::vector<Message>& messages) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        LOG_ERROR << "MessageStorageManager not initialized";
        return false;
    }

    if (messages.empty()) {
        return true;
    }

    try {
        CDBConn* db_conn = getDBConnection();
        if (!db_conn) {
            LOG_ERROR << "Failed to get database connection";
            return false;
        }

        // 使用RAII自动释放连接
        auto conn_guard = [this](CDBConn* conn) { releaseDBConnection(conn); };
        std::unique_ptr<CDBConn, decltype(conn_guard)> conn_ptr(db_conn, conn_guard);

        string sql = "INSERT INTO "
            "message_infos (`msg_id`, `room_id`, `user_id`, `username`, `msg_content`) VALUES ";

        std::vector<string> value_parts;
        for (size_t i = 0; i < messages.size(); ++i) {
            value_parts.push_back("(?, ?, ?, ?, ?)");
        }

        sql += std::accumulate(value_parts.begin(), value_parts.end(), string(),
            [](const string& a, const string& b) {
                return a.empty() ? b : a + ", " + b;
            });

        CPrepareStatement stmt;
        if (!stmt.Init(db_conn->GetMysql(), sql)) {
            LOG_ERROR << "Failed to prepare batch SQL statement";
            return false;
        }

        // 为需要处理的字段创建副本
        std::vector<string> msg_ids;

        msg_ids.reserve(messages.size());

        for (const auto& msg : messages) {
            msg_ids.emplace_back(msg.id.empty() ? generateMessageId() : msg.id);
        }

        int param_index = 0;
        for (size_t i = 0; i < messages.size(); ++i) {
            stmt.SetParam(param_index++, msg_ids[i]);
            stmt.SetParam(param_index++, room_id);
            stmt.SetParam(param_index++, messages[i].user_id);
            stmt.SetParam(param_index++, messages[i].username);
            stmt.SetParam(param_index++, messages[i].content);
        }

        if (!stmt.ExecuteUpdate()) {
            LOG_ERROR << "Failed to execute batch SQL statement";
            return false;
        }

        LOG_INFO << "Batch stored " << messages.size() << " messages to database successfully";
        return true;

    }
    catch (const std::exception& e) {
        LOG_ERROR << "Error in batch storing messages to database: " << e.what();
        return false;
    }
}