#pragma once
#include <string>
#include <vector>
#include "muduo/base/Logging.h"

struct UserInfo {
    std::string email;
    std::string password;
    std::string room_id;
};

class ConfigManager {
public:
    static ConfigManager& getInstance() {
        static ConfigManager instance;
        return instance;
    }

    bool loadConfig(const std::string& filename);
    std::vector<UserInfo> getUsers();
    bool hasEnoughUsers(int required_count);

private:
    ConfigManager() {}
    std::vector<UserInfo> users_;
}; 