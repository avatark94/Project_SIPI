#include "UserManager.h"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <vector>

// Вспомогательная функция: hex-строка из байтов
static std::string bytesToHex(const unsigned char* data, size_t len) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (size_t i = 0; i < len; ++i) {
        ss << std::setw(2) << static_cast<int>(data[i]);
    }
    return ss.str();
}

// Вспомогательная функция: байты из hex-строки
static std::vector<unsigned char> hexToBytes(const std::string& hex) {
    std::vector<unsigned char> bytes;
    for (size_t i = 0; i < hex.length(); i += 2) {
        std::string byteString = hex.substr(i, 2);
        unsigned char byte = static_cast<unsigned char>(std::stoi(byteString, nullptr, 16));
        bytes.push_back(byte);
    }
    return bytes;
}

// Хеширование пароля: возвращает "соль:хеш" в hex
static std::string hashPassword(const std::string& password) {
    unsigned char salt[16];
    if (RAND_bytes(salt, sizeof(salt)) != 1) {
        throw std::runtime_error("RAND_bytes failed");
    }
    unsigned char hash[32]; // SHA-256 даёт 32 байта
    // PKCS5_PBKDF2_HMAC (итерации 10000, SHA-256)
    if (!PKCS5_PBKDF2_HMAC(password.c_str(), password.length(),
                           salt, sizeof(salt),
                           10000, EVP_sha256(),
                           sizeof(hash), hash)) {
        throw std::runtime_error("PKCS5_PBKDF2_HMAC failed");
    }
    return bytesToHex(salt, sizeof(salt)) + ":" + bytesToHex(hash, sizeof(hash));
}

// Проверка пароля
static bool verifyPassword(const std::string& password, const std::string& stored) {
    size_t colon = stored.find(':');
    if (colon == std::string::npos) return false;
    std::string saltHex = stored.substr(0, colon);
    std::string hashHex = stored.substr(colon + 1);
    std::vector<unsigned char> salt = hexToBytes(saltHex);
    if (salt.size() != 16) return false;
    unsigned char computedHash[32];
    if (!PKCS5_PBKDF2_HMAC(password.c_str(), password.length(),
                           salt.data(), salt.size(),
                           10000, EVP_sha256(),
                           sizeof(computedHash), computedHash)) {
        return false;
    }
    std::string computedHex = bytesToHex(computedHash, sizeof(computedHash));
    return computedHex == hashHex;
}

// Реализация методов UserManager
UserManager::UserManager(Database& db) : db(db) {}

bool UserManager::initTables() {
    std::string sql = R"(
        CREATE TABLE IF NOT EXISTS users (
            id SERIAL PRIMARY KEY,
            email TEXT UNIQUE NOT NULL,
            password_hash TEXT NOT NULL,
            name TEXT,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        );
    )";
    return db.exec(sql);
}

std::optional<int> UserManager::registerUser(const std::string& email, const std::string& password, const std::optional<std::string>& name) {
    // Проверка существования
    std::stringstream check;
    check << "SELECT id FROM users WHERE email = '" << email << "';";
    PGresult* resCheck = db.query(check.str());
    if (PQntuples(resCheck) > 0) {
        PQclear(resCheck);
        return std::nullopt;
    }
    PQclear(resCheck);

    std::string hashed = hashPassword(password);
    std::stringstream sql;
    sql << "INSERT INTO users (email, password_hash, name) VALUES ('"
        << email << "', '" << hashed << "', ";
    if (name) sql << "'" << *name << "')";
    else sql << "NULL)";
    sql << " RETURNING id;";
    PGresult* res = db.query(sql.str());
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        return std::nullopt;
    }
    int userId = std::stoi(PQgetvalue(res, 0, 0));
    PQclear(res);
    return userId;
}

std::optional<int> UserManager::authenticateUser(const std::string& email, const std::string& password) {
    std::stringstream sql;
    sql << "SELECT id, password_hash FROM users WHERE email = '" << email << "';";
    PGresult* res = db.query(sql.str());
    if (PQntuples(res) != 1) {
        PQclear(res);
        return std::nullopt;
    }
    int userId = std::stoi(PQgetvalue(res, 0, 0));
    std::string hash = PQgetvalue(res, 0, 1);
    PQclear(res);
    if (verifyPassword(password, hash)) {
        return userId;
    }
    return std::nullopt;
}