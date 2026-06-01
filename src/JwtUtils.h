#pragma once
#include <string>

class JwtUtils {
public:
    static std::string generateToken(int userId, long expiresSeconds = 3600);
    static int verifyToken(const std::string& token);
};