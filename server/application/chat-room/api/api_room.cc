#include<sstream>
#include "api_room.h"


bool ApiCreateRoom(const std::string& room_id, const string& room_name, int creator_id, string& error_msg) {
    CDBManager* db_manager = CDBManager::getInstance();
    CDBConn* db_conn = db_manager->GetDBConn("chatroom_master");
    if (!db_conn) {
        error_msg = "get CDBConn failed";
        return false;
    }
    AUTO_REL_DBCONN(db_manager, db_conn);

    std::stringstream ss;
    ss << "INSERT INTO room_info (room_id, room_name, creator_id) VALUES ('"
        << room_id << "', '"
        << room_name << "', "
        << creator_id << ")";

    if (!db_conn->ExecuteUpdate(ss.str().c_str(), true)) {
        error_msg = "create chatroom failed";
        return false;
    }

    return true;
}

bool ApiGetRoomInfo(const std::string& room_id, string& room_name, int& creator_id, string& create_time,
    string& update_time, string& error_msg) {
    CDBManager* db_manager = CDBManager::getInstance();
    CDBConn* db_conn = db_manager->GetDBConn("chatroom_slave");
    if (!db_conn) {
        error_msg = "get CDBConn failed";
        return false;
    }
    AUTO_REL_DBCONN(db_manager, db_conn);

    std::stringstream ss;

    ss << "SELECT room_name, creator_id, create_time, update_time FROM room_info WHERE room_id = '" << room_id << "'";
    CResultSet* res_set = db_conn->ExecuteQuery(ss.str().c_str());

    if (!res_set) {
        error_msg = "get chatroom info failed";
        return false;
    }

    if (res_set->Next()) {
        room_name = res_set->GetString("room_name");
        creator_id = res_set->GetInt("creator_id");
        create_time = res_set->GetString("create_time");
        update_time = res_set->GetString("update_time");
        delete res_set;
        return true;
    }
    delete res_set;
    error_msg = "chatroom not exist";

    return false;
}

bool ApiGetAllRooms(std::vector<Room>& rooms, string& error_msg, const string& order_by) {
    CDBManager* db_manager = CDBManager::getInstance();
    CDBConn* db_conn = db_manager->GetDBConn("chatroom_slave");
    if (!db_conn) {
        error_msg = "get CDBConn failed";
        return false;
    }
    AUTO_REL_DBCONN(db_manager, db_conn);

    std::stringstream ss;
    ss << "SELECT room_id, room_name, creator_id, create_time, update_time FROM room_info ORDER BY " << order_by;
    CResultSet* res_set = db_conn->ExecuteQuery(ss.str().c_str());

    if (!res_set) {
        error_msg = "get all chatrooms failed";
        return false;
    }

    while (res_set->Next()) {
        Room room;
        room.room_id = res_set->GetString("room_id");
        room.room_name = res_set->GetString("room_name");
        room.creator_id = res_set->GetInt("creator_id");
        room.create_time = res_set->GetString("create_time");
        room.update_time = res_set->GetString("update_time");
        rooms.push_back(room);
    }

    delete res_set;
    return true;
}