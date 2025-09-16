#include "config_manager.h"
#include <fstream>
#include <sstream>

bool ConfigManager::loadConfig(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        LOG_ERROR << "无法打开配置文件: " << filename;
        return false;
    }

    users_.clear();
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string email, password, room_id;
        if (std::getline(iss, email, '=') && 
            std::getline(iss, password, '=') && 
            std::getline(iss, room_id)) {
            users_.push_back({email, password, room_id});
        }
    }
    return true;
}
 

std::vector<UserInfo> ConfigManager::getUsers() {
    return users_;
}


bool ConfigManager::hasEnoughUsers(int required_count) {
    return users_.size() >= required_count;
} 