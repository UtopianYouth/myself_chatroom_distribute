#ifndef __ROOM_SERVICE_H__
#define __ROOM_SERVICE_H__

#include <vector>
#include <string>
#include <mutex>
#include "../api/api_types.h"

using namespace std;

/**
 * 房间服务 - Logic层房间业务逻辑的统一管理
 * 整合了房间的业务逻辑、数据库操作和内存管理
 */
class RoomService {
public:
    // 单例模式
    static RoomService& getInstance() {
        static RoomService instance;
        return instance;
    }

    /**
     * 初始化房间服务
     * 1. 将默认房间插入数据库（如果不存在）
     * 2. 从数据库获取所有房间并加载到内存
     */
    int initialize();

    /**
     * 创建新房间
     * @param room_name 房间名称
     * @param creator_id 创建者ID
     * @param creator_username 创建者用户名
     * @param room_id 输出参数，新创建的房间ID
     * @param error_msg 错误信息
     * @return 成功返回0，失败返回-1
     */
    int createRoom(const string& room_name, int64_t creator_id, 
                   const string& creator_username, string& room_id, string& error_msg);

    /**
     * 获取所有房间信息（从数据库）
     * @param rooms 输出参数，房间列表
     * @param error_msg 错误信息
     * @param order_by 排序方式
     * @return 成功返回true，失败返回false
     */
    bool getAllRoomsFromDB(std::vector<Room>& rooms, string& error_msg, const string& order_by = "update_time DESC");

    /**
     * 获取房间信息（从数据库）
     * @param room_id 房间ID
     * @param room_name 输出参数，房间名称
     * @param creator_id 输出参数，创建者ID
     * @param create_time 输出参数，创建时间
     * @param update_time 输出参数，更新时间
     * @param error_msg 错误信息
     * @return 成功返回true，失败返回false
     */
    bool getRoomInfoFromDB(const string& room_id, string& room_name, int& creator_id, 
                     string& create_time, string& update_time, string& error_msg);

    /**
     * 获取内存中的房间列表（用于快速访问）
     * @return 房间列表引用
     */
    std::vector<Room>& getRoomList();

    /**
     * 添加房间到内存缓存
     * @param room 房间信息
     * @return 成功返回0，失败返回-1
     */
    int addRoomToList(const Room& room);

    // 重新加载房间列表
    int refreshRoomList();

private:
    RoomService() = default;
    ~RoomService() = default;
    RoomService(const RoomService&) = delete;
    RoomService& operator=(const RoomService&) = delete;

    bool dbCreateRoom(const std::string& room_id, const string& room_name, int creator_id, string& error_msg);
    bool dbGetRoomInfo(const std::string& room_id, string& room_name, int& creator_id, 
                       string& create_time, string& update_time, string& error_msg);
    bool dbGetAllRooms(std::vector<Room>& rooms, string& error_msg, const string& order_by = "update_time DESC");

    // 默认房间列表
    std::vector<Room> m_default_rooms = {
        {"0001", "Linux Study Group", 1, "", "", ""},
        {"0002", "C++ Study Group", 2, "", "", ""},
        {"0003", "ThreadPool Study Group", 3, "", "", ""},
        {"0004", "Database Study Group", 4, "", "", ""},
        {"0005", "Network Study Group", 5, "", "", ""},
        {"0006", "Redis Study Group", 6, "", "", ""},
        {"0007", "Nginx Study Group", 7, "", "", ""},
        {"0008", "Git Study Group", 8, "", "", ""}
    };

    // 房间列表
    std::vector<Room> m_room_list;
    mutable std::mutex m_room_list_mtx;
    
    static constexpr int SYSTEM_USER_ID = 1;
};

#endif // __ROOM_SERVICE_H__