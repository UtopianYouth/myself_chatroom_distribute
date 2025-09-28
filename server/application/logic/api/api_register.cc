#include "api_register.h"
#include "api_common.h"
#include "muduo/base/Logging.h"
#include "muduo/base/md5.h"
#include <json/json.h>

/**
 * 解析注册JSON请求
 * @return: parse success 0, parse failed -1
 */
int DecodeRegisterJson(const string& str_json, string& username,
    string& email, string& password) {

    bool res;
    Json::Value root;
    Json::Reader jsonReader;

    res = jsonReader.parse(str_json, root);

    if (!res) {
        LOG_ERROR << "parse register json failed";
        return -1;
    }

    if (root["username"].isNull()) {
        LOG_ERROR << "register username null";
        return -1;
    }
    username = root["username"].asString();

    if (root["email"].isNull()) {
        LOG_ERROR << "register email null";
        return -1;
    }
    email = root["email"].asString();

    if (root["password"].isNull()) {
        LOG_ERROR << "register password null";
        return -1;
    }
    password = root["password"].asString();

    return 0;
}

/**
 * 编码注册响应JSON
 */
void EncodeRegisterJson(api_error_id input, string message, string& str_json) {
    Json::Value root;
    root["id"] = ApiErrorIdToString(input);
    root["message"] = message;
    Json::FastWriter writer;
    str_json = writer.write(root);
}

/**
 * 注册用户
 * @return: -1,username or email exist; 1,insert sql failed; 0,register success
 */
int RegisterUser(string& username, string& email, string& password, api_error_id& error_id) {
    int ret = -1;

    // 获取数据库连接
    CDBManager* db_manager = CDBManager::getInstance();
    CDBConn* db_conn = db_manager->GetDBConn("chatroom_master");
    AUTO_REL_DBCONN(db_manager, db_conn);
    
    if (!db_conn) {
        LOG_ERROR << "GetDBConn(chatroom_master) failed";
        return -1;
    }

    // 检查用户名和邮箱是否已存在
    string str_sql = FormatString("select id, username, email from user_infos where username='%s' or email = '%s' ",
        username.c_str(), email.c_str());
    CResultSet* result_set = db_conn->ExecuteQuery(str_sql.c_str());
    
    if (result_set && result_set->Next()) {
        if (result_set->GetString("username") == username) {
            error_id = api_error_id::username_exists;
            LOG_WARN << "username: " << username << " exists";
        }
        if (result_set->GetString("email") == email) {
            error_id = api_error_id::email_exists;
            LOG_WARN << "email: " << email << " exists";
        }
        delete result_set;
        return -1;
    }

    // 生成盐值和密码哈希
    string salt = RandomString(16);
    MD5 md5(password + salt);
    string password_hash = md5.toString();
    LOG_INFO << "salt: " << salt;

    // 生成用户ID (UUID)
    string user_id = GenerateUUID();
    
    // 插入新用户
    str_sql = "insert into user_infos(`user_id`, `username`, `email`, `password_hash`, `salt`) values(?,?,?,?,?)";
    LOG_INFO << "exec: " << str_sql;

    CPrepareStatement* stmt = new CPrepareStatement();
    if (stmt->Init(db_conn->GetMysql(), str_sql)) {
        uint32_t index = 0;
        stmt->SetParam(index++, user_id);
        stmt->SetParam(index++, username);
        stmt->SetParam(index++, email);
        stmt->SetParam(index++, password_hash);
        stmt->SetParam(index++, salt);

        bool bRet = stmt->ExecuteUpdate();
        if (bRet) {
            ret = 0;
            LOG_INFO << "insert user_id: " << db_conn->GetInsertId() << ", username: " << username;
        } else {
            ret = 1;
            LOG_ERROR << "insert users failed: " << str_sql;
        }
    }

    delete stmt;
    return ret;
}

/**
 * 用户注册API
 */
int ApiRegisterUser(string& post_data, string& response_data) {
    string username;
    string email;
    string password;

    // 解析注册请求JSON
    int ret = DecodeRegisterJson(post_data, username, email, password);
    if (ret < 0) {
        EncodeRegisterJson(api_error_id::bad_request, "the parameter of request is not enough", response_data);
        return -1;
    }

    // 注册用户
    api_error_id error_id = api_error_id::bad_request;
    ret = RegisterUser(username, email, password, error_id);

    if (ret == 0) {
        // 注册成功，设置cookie
        ApiSetCookie(email, response_data);
        LOG_INFO << "user register success, email: " << email;
    } else {
        // 注册失败
        EncodeRegisterJson(error_id, ApiErrorIdToString(error_id), response_data);
        LOG_WARN << "user register failed, email: " << email;
    }

    return ret;
}