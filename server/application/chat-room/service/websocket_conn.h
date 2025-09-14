#ifndef __WEBSOCKET_CONN_H__
#define __WEBSOCKET_CONN_H__

#include <sstream>           
#include <openssl/sha.h>
#include "http_conn.h"
#include "muduo/base/Logging.h" 
#include"api_types.h"

class CWebSocketConn : public CHttpConn {
public:
    CWebSocketConn(const TcpConnectionPtr& conn);
    virtual ~CWebSocketConn();
    virtual void OnRead(Buffer* buf);
    void Disconnect();

private:
    int64_t user_id = -1;       // userid
    string username;            // username
    bool handshake_completed = false;   // websocket conn has completed?   
    std::unordered_map<string, Room> rooms_map;    // has joined the chatrooms

    void SendCloseFrame(uint16_t code, const string& reason);
    void SendPongFrame();       // Pong frame
    int SendHelloMessage();

    int HandleClientMessages(Json::Value& root);
    int HandleRequestRoomHistory(Json::Value& root);
    int HandleClientCreateRoom(Json::Value& root);
};

using CWebSocketConnPtr = std::shared_ptr<CWebSocketConn>;

#endif // !__WEBSOCKET_CONN_H__