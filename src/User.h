#pragma once
#include <string>
#include <optional>

struct User {
    int id;
    std::string email;
    std::string password_hash;
    std::optional<std::string> name;
    std::string created_at;
};