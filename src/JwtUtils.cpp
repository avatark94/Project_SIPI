#include "JwtUtils.h"
#include <jwt-cpp/jwt.h>
#include <chrono>
#include <iostream>
#include <cstdlib>   // для std::getenv
#include <stdexcept>

static std::string getJwtSecret() {
    const char* secret = std::getenv("JWT_SECRET");
    if (!secret || strlen(secret) == 0) {
        throw std::runtime_error("JWT_SECRET environment variable not set");
    }
    return std::string(secret);
}

std::string JwtUtils::generateToken(int userId, long expiresSeconds) {
    auto now = std::chrono::system_clock::now();
    auto expires = now + std::chrono::seconds(expiresSeconds);
    auto token = jwt::create()
        .set_issuer("taskplanner")
        .set_type("JWT")
        .set_payload_claim("user_id", jwt::claim(std::to_string(userId)))
        .set_issued_at(now)
        .set_expires_at(expires)
        .sign(jwt::algorithm::hs256{getJwtSecret()});
    return token;
}

int JwtUtils::verifyToken(const std::string& token) {
    try {
        auto decoded = jwt::decode(token);
        auto verifier = jwt::verify()
            .allow_algorithm(jwt::algorithm::hs256{getJwtSecret()})
            .with_issuer("taskplanner");
        verifier.verify(decoded);
        auto claim = decoded.get_payload_claim("user_id");
        return std::stoi(claim.as_string());
    } catch (const std::exception& e) {
        std::cerr << "JWT error: " << e.what() << std::endl;
        return -1;
    }
}