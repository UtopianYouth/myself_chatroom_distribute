#include "api_register.h"

/**
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
 * encode json data for response
 *
 */
void EncodeRegisterJson(api_error_id input, string message, string& str_json) {
    Json::Value root;
    root["id"] = api_error_id_to_string(input);
    root["message"] = message;
    Json::FastWriter writer;

    str_json = writer.write(root);
}

/**
 * @return: -1,username or email exist; 1,insert sql failed; 0,register success
 *
 */
int RegisterUser(string& username, string& email, string& password, api_error_id& error_id) {
    int ret = -1;

    // 0. get mysql connection
    CDBManager* db_manager = CDBManager::getInstance();
    CDBConn* db_conn = db_manager->GetDBConn("chatroom_master");
    AUTO_REL_DBCONN(db_manager, db_conn);
    if (!db_conn) {
        LOG_ERROR << "GetDBConn(chatroom_master) failed";
        return -1;
    }

    // 1. read username and email exist in mysql? ==> exist: return -1(error)
    string str_sql = FormatString("select id, username, email from users where username='%s' or email = '%s' ",
        username.c_str(), email.c_str());
    CResultSet* result_set = db_conn->ExecuteQuery(str_sql.c_str());
    if (result_set && result_set->Next()) {
        if (result_set->GetString("username")) {
            if (result_set->GetString("username") == username) {
                error_id = api_error_id::username_exists;
                LOG_WARN << "id: " << result_set->GetInt("id") << ", username: " << username << " exists";
            }
        }

        // if username and email both exists, return api_error_id::email_exists
        if (result_set->GetString("email")) {
            if (result_set->GetString("email") == email) {
                error_id = api_error_id::email_exists;
                LOG_WARN << "id: " << result_set->GetInt("id") << ", email: " << email << " exists";
            }
        }

        delete result_set;
        return -1;
    }

    // 2. register username and email, generating salt value
    string salt = RandomString(16);
    MD5 md5(password + salt);
    string password_hash = md5.toString();
    LOG_INFO << "salt: " << salt;

    // 3. exec insert sql
    str_sql = "insert into users(`username`, `email`, `password_hash`, `salt`) values(?,?,?,?)";
    LOG_INFO << "exec: " << str_sql;

    // 3.1 PrepareStatement write data to mysql
    CPrepareStatement* stmt = new CPrepareStatement();
    if (stmt->Init(db_conn->GetMysql(), str_sql)) {
        uint32_t index = 0;
        stmt->SetParam(index++, username);
        stmt->SetParam(index++, email);
        stmt->SetParam(index++, password_hash);
        stmt->SetParam(index++, salt);

        bool bRet = stmt->ExecuteUpdate();
        if (bRet) {
            ret = 0;
            LOG_INFO << "insert user_id: " << db_conn->GetInsertId() << ", username: " << username;
        }
        else {
            ret = 1;
            LOG_ERROR << "insert users failed: " << str_sql;
        }
    }

    delete stmt;
    return ret;
}

/**
 *
 */
int ApiRegisterUser(string& post_data, string& response_data) {
    string username;
    string email;
    string password;

    // parse json ==> username, email, password
    int ret = DecodeRegisterJson(post_data, username, email, password);
    if (ret < 0) {
        EncodeRegisterJson(api_error_id::bad_request, "the parameter of request is not enough", response_data);
        return -1;
    }

    // call RegisterUser
    api_error_id error_id = api_error_id::bad_request;
    ret = RegisterUser(username, email, password, error_id);

    if (ret == 0) {
        // register success
        ApiSetCookie(email, response_data);
    }
    else {
        EncodeRegisterJson(error_id, api_error_id_to_string(error_id), response_data);
    }

    // register success, return cookie

    // exception{} 400 Bad Request {"id":"USERNAME_EXISTS", "message":""}    EMAIL_EXISTS

    return ret;
}