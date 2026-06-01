#include "Database.h"
#include <iostream>

Database::Database() : conn(nullptr) {}

Database::~Database() {
    close();
}

bool Database::open(const std::string& conninfo) {
    conn = PQconnectdb(conninfo.c_str());
    if (PQstatus(conn) != CONNECTION_OK) {
        std::cerr << "Ошибка подключения к PostgreSQL: " << PQerrorMessage(conn) << std::endl;
        return false;
    }
    return true;
}

void Database::close() {
    if (conn) {
        PQfinish(conn);
        conn = nullptr;
    }
}

bool Database::exec(const std::string& sql) {
    PGresult* res = PQexec(conn, sql.c_str());
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        std::cerr << "SQL ошибка: " << PQerrorMessage(conn) << std::endl;
        std::cerr << "Запрос: " << sql << std::endl;
        PQclear(res);
        return false;
    }
    PQclear(res);
    return true;
}

PGresult* Database::query(const std::string& sql) {
    return PQexec(conn, sql.c_str());
}