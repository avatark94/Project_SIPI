#pragma once
#include "Database.h"
#include <string>
#include <optional>

class UserManager {
public:
    UserManager(Database& db);
    bool initTables();  // создаёт таблицу users
    std::optional<int> registerUser(const std::string& email, const std::string& password, const std::optional<std::string>& name);
    std::optional<int> authenticateUser(const std::string& email, const std::string& password);
private:
    Database& db;
};