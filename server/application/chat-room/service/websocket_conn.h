#ifndef __WEBSOCKET_CONN_H__
#define __WEBSOCKET_CONN_H__

#include <sstream>           
#include <openssl/sha.h>
#include "http_conn.h"
#include "muduo/base/Logging.h" 
#include "muduo/base/ThreadPool.h"
#include"api_types.h"

class CWebSocketConn : public CHttpConn {
public:
    static void InitThreadPool(int thread_num);

    CWebSocketConn(const TcpConnectionPtr& conn);
    virtual ~CWebSocketConn();
    virtual void OnRead(Buffer* buf);
    void Disconnect();
private:
    string user_id;             // userid (UUID)
    string username;            // username
    bool handshake_completed = false;               // websocket conn has completed
    std::unordered_map<string, Room> rooms_map;     // has joined the chatrooms
    string incomplete_frame_buffer;                 // store incomplete websocket frame
    uint64_t stats_total_messages = 0;
    uint64_t stats_total_bytes = 0;
    static ThreadPool* s_thread_pool;

    void SendCloseFrame(uint16_t code, const string& reason);
    void SendPongFrame();       // Pong frame
    int SendHelloMessage();

    int HandleClientMessages(Json::Value& root);
    int HandleRequestRoomHistory(Json::Value& root);
    int HandleClientCreateRoom(Json::Value& root);
};

using CWebSocketConnPtr = std::shared_ptr<CWebSocketConn>;

extern std::unordered_map<string, CHttpConnPtr> s_user_ws_conn_map;       // store user id and websocket conn
extern std::mutex s_mtx_user_ws_conn_map;

string BuildWebSocketFrame(const string& payload, const uint8_t opcode = 0x01);

#endif // !__WEBSOCKET_CONN_H__