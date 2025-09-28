#include "room_service.h"
#include "../base/config_file_reader.h"
#include "../mysql/db_pool.h"
#include "../api/api_common.h"
#include "muduo/base/Logging.h"
#include <sstream>

using namespace muduo;

int RoomService::initialize() {
    std::string err_msg;

    // 1. 将默认房间插入数据库
    std::lock_guard<std::mutex> lock(m_room_list_mtx);
    for (const Room& room : m_default_rooms) {
        string room_id = room.room_id;
        string existing_room_name;
        string creator_id;
        string create_time;
        string update_time;
        
        // 检查房间是否已存在
        if (!dbGetRoomInfo(room_id, existing_room_name, creator_id,
             create_time, update_time, err_msg)) {

            if (!dbCreateRoom(room.room_id, room.room_name,
                 "1", err_msg)) {
                LOG_ERROR << "Failed to create default room: " << room.room_id 
                         << ", room_name: " << room.room_name << ", error: " << err_msg;
                continue;
            }

            LOG_INFO << "Created default room: " << room.room_id << ", room_name: " << room.room_name;
        }
    }

    // 2. 更新 m_room_list
    if (refreshRoomList() < 0) {
        LOG_ERROR << "Failed to refresh m_room_list";
        return -1;
    }
    LOG_INFO << "RoomService initialization completed, total rooms: " << m_room_list.size();

    return 0;
}

int RoomService::createRoom(const string& room_name, const string& creator_id, 
                           const string& creator_username, string& room_id, string& error_msg) {
    // 生成UUID作为房间ID
    room_id = GenerateUUID();

    if (!dbCreateRoom(room_id, room_name, creator_id, error_msg)) {
        LOG_ERROR << "Failed to create room in database: " << error_msg;
        return -1;
    }

    Room new_room;
    new_room.room_id = room_id;
    new_room.room_name = room_name;
    new_room.creator_id = creator_id;

    if (addRoomToList(new_room) < 0) {
        LOG_WARN << "Room already exists in cache: " << room_id;
    }

    LOG_INFO << "Room created successfully: " << room_id << ", name: " << room_name 
             << ", creator: " << creator_username << "(" << creator_id << ")";

    return 0;
}

bool RoomService::getRoomInfoFromDB(const string& room_id, string& room_name, string& creator_id, 
                             string& create_time, string& update_time, string& error_msg) {
    return dbGetRoomInfo(room_id, room_name, creator_id, create_time, update_time, error_msg);
}

bool RoomService::getAllRoomsFromDB(std::vector<Room>& rooms, string& error_msg, const string& order_by) {
    return dbGetAllRooms(rooms, error_msg, order_by);
}

std::vector<Room>& RoomService::getRoomList() {
    std::lock_guard<std::mutex> lock(m_room_list_mtx);
    return m_room_list;
}

int RoomService::addRoomToList(const Room& room) {
    std::lock_guard<std::mutex> lock(m_room_list_mtx);

    // 检查房间是否已存在
    for (const Room& r : m_room_list) {
        if (r.room_id == room.room_id) {
            LOG_DEBUG << "Room already exists in cache: " << r.room_id;
            return -1;
        }
    }
    
    m_room_list.push_back(room);
    LOG_INFO << "Room added to m_room_list: " << room.room_id << ", name: " << room.room_name;

    return 0;
}

int RoomService::refreshRoomList() {
    std::string err_msg;
    std::vector<Room> all_rooms;
    
    if (!dbGetAllRooms(all_rooms, err_msg, "update_time DESC")) {
        LOG_ERROR << "Failed to get all rooms from database: " << err_msg;
        return -1;
    }
    m_room_list.clear();

    for (const auto& room : all_rooms) {
        m_room_list.push_back(room);
    }

    LOG_INFO << "Room cache refreshed, total rooms: " << m_room_list.size();
    return 0;
}


// ======================PRIVATE=======================
bool RoomService::dbCreateRoom(const std::string& room_id, const string& room_name, const string& creator_id,
    string& error_msg) {
    CDBManager* db_manager = CDBManager::getInstance();
    if (!db_manager) {
        error_msg = "Database manager not initialized";
        return false;
    }
    
    CDBConn* db_conn = db_manager->GetDBConn("chatroom_master");
    if (!db_conn) {
        error_msg = "Failed to get database connection";
        return false;
    }
    AUTO_REL_DBCONN(db_manager, db_conn);

    std::stringstream ss;
    ss << "INSERT INTO room_infos (room_id, room_name, creator_id) VALUES ('"
        << room_id << "', '"
        << room_name << "', '"
        << creator_id << "')";

    if (!db_conn->ExecuteUpdate(ss.str().c_str(), true)) {
        error_msg = "Failed to execute room creation SQL";
        return false;
    }

    return true;
}

bool RoomService::dbGetRoomInfo(const std::string& room_id, string& room_name, string& creator_id, 
                               string& create_time, string& update_time, string& error_msg) {
    CDBManager* db_manager = CDBManager::getInstance();
    if (!db_manager) {
        error_msg = "Database manager not initialized";
        return false;
    }
    
    CDBConn* db_conn = db_manager->GetDBConn("chatroom_slave");
    if (!db_conn) {
        error_msg = "Failed to get database connection";
        return false;
    }
    AUTO_REL_DBCONN(db_manager, db_conn);

    std::stringstream ss;
    ss << "SELECT room_name, creator_id, create_time, update_time FROM room_infos WHERE room_id = '" << room_id << "'";
    CResultSet* res_set = db_conn->ExecuteQuery(ss.str().c_str());

    if (!res_set) {
        error_msg = "Failed to execute room query SQL";
        return false;
    }

    if (res_set->Next()) {
        room_name = res_set->GetString("room_name");
        creator_id = res_set->GetString("creator_id");
        create_time = res_set->GetString("create_time");
        update_time = res_set->GetString("update_time");
        delete res_set;
        return true;
    }
    
    delete res_set;
    error_msg = "Room not found";
    return false;
}

bool RoomService::dbGetAllRooms(std::vector<Room>& rooms, string& error_msg, const string& order_by) {
    CDBManager* db_manager = CDBManager::getInstance();
    if (!db_manager) {
        error_msg = "Database manager not initialized";
        return false;
    }
    
    CDBConn* db_conn = db_manager->GetDBConn("chatroom_slave");
    if (!db_conn) {
        error_msg = "Failed to get database connection";
        return false;
    }
    AUTO_REL_DBCONN(db_manager, db_conn);

    std::stringstream ss;
    ss << "SELECT room_id, room_name, creator_id, create_time, update_time FROM room_infos ORDER BY " << order_by;
    CResultSet* res_set = db_conn->ExecuteQuery(ss.str().c_str());

    if (!res_set) {
        error_msg = "Failed to execute rooms query SQL";
        return false;
    }

    rooms.clear();
    while (res_set->Next()) {
        Room room;
        room.room_id = res_set->GetString("room_id");
        room.room_name = res_set->GetString("room_name");
        room.creator_id = res_set->GetString("creator_id");
        room.create_time = res_set->GetString("create_time");
        room.update_time = res_set->GetString("update_time");
        rooms.push_back(room);
    }

    delete res_set;
    return true;
}