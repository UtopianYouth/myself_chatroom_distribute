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
    virtual void OnRead(Buffer* buf);
    virtual ~CWebSocketConn();
private:
    void SendCloseFrame(uint16_t code, const std::string& reason);
    void SendPongFrame();       // Pong frame
    void Disconnect();
    bool IsCloseFrame(const std::string& frame);

    void SendHelloMessage();
    void HandleClientMessage(Json::Value& root);

    bool handshake_completed = false;   // the websocket conn of client with server 
    string username;           // username
    int32_t userid = -1;       // userid
    std::unordered_map<string, Room> rooms_map;    //   join the chat-rooms 
};

using CWebSocketConnPtr = std::shared_ptr<CWebSocketConn>;

#endif