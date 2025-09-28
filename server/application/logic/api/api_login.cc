#include "api_login.h"
#include "api_common.h"
#include "muduo/base/Logging.h"
#include "muduo/base/md5.h"
#include <json/json.h>

/**
 * 解析登录JSON请求
 * @return: 0,decode json success; -1,decode json failed
 */
int DecodeLoginJson(const string& str_json, string& email, string& password) {
    bool res;
    Json::Value root;
    Json::Reader jsonReader;
    res = jsonReader.parse(str_json, root);

    if (!res) {
        LOG_ERROR << "parse login json failed";
        return -1;
    }

    if (root["email"].isNull()) {
        LOG_ERROR << "email null";
        return -1;
    }
    email = root["email"].asString();

    if (root["password"].isNull()) {
        LOG_ERROR << "password null";
        return -1;
    }
    password = root["password"].asString();

    return 0;
}

/**
 * 编码登录响应JSON
 */
int EncodeLoginJson(api_error_id input, string message, string& str_json) {
    Json::Value root;
    root["id"] = ApiErrorIdToString(input);
    root["message"] = message;
    Json::FastWriter writer;
    str_json = writer.write(root);
    return 0;
}

/**
 * 验证用户密码
 * @return: 0,verify success; -1,verify failed
 */
int VerifyUserPassword(string& email, string& password) {
    int ret = -1;

    CDBManager* db_manager = CDBManager::getInstance();
    CDBConn* db_conn = db_manager->GetDBConn("chatroom_slave");
    AUTO_REL_DBCONN(db_manager, db_conn);

    if (!db_conn) {
        LOG_ERROR << "get db conn failed";
        return -1;
    }

    // 读取用户密码信息
    string strSql = FormatString("select user_id, username, password_hash, salt from user_infos where email='%s'", email.c_str());
    LOG_INFO << "exec: " << strSql;
    CResultSet* result_set = db_conn->ExecuteQuery(strSql.c_str());
    
    if (result_set && result_set->Next()) {
        string username = result_set->GetString("username");
        string db_password_hash = result_set->GetString("password_hash");
        string salt = result_set->GetString("salt");
        
        // 计算客户端密码哈希
        MD5 md5(password + salt);
        string client_password_hash = md5.toString();
        
        if (db_password_hash == client_password_hash) {
            LOG_INFO << "username: " << username << " verify ok";
            ret = 0;
        } else {
            LOG_INFO << "username: " << username << " verify failed";
            ret = -1;
        }
    } else {
        LOG_WARN << "user not found for email: " << email;
        ret = -1;
    }

    if (result_set) {
        delete result_set;
    }

    return ret;
}

/**
 * 用户登录 API
 * @return: 0, login success; -1,login failed
 */
int ApiLoginUser(string& post_data, string& response_data) {
    string email;
    string password;

    // 解析登录请求JSON
    if (DecodeLoginJson(post_data, email, password) < 0) {
        LOG_ERROR << "decode login json failed";
        EncodeLoginJson(api_error_id::bad_request, "email or password no fill", response_data);
        return -1;
    }

    // 验证用户密码
    int ret = VerifyUserPassword(email, password);

    if (ret == 0) {
        // 登录成功，设置cookie
        ApiSetCookie(email, response_data);
        LOG_INFO << "user login success, email: " << email;
    } else {
        // 登录失败
        EncodeLoginJson(api_error_id::login_failed, "email password no match", response_data);
        LOG_WARN << "user login failed, email: " << email;
    }
    
    return ret;
}