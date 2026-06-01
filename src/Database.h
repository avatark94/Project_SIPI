/**
 * @file Database.h
 * @brief Обёртка для работы с PostgreSQL (libpq).
 */
#pragma once

#include <libpq-fe.h>
#include <string>

class Database {
public:
    Database();
    ~Database();

    /**
     * @brief Подключиться к PostgreSQL.
     * @param conninfo Строка подключения (например, "host=localhost dbname=taskplanner user=taskuser password=123")
     * @return true при успехе.
     */
    bool open(const std::string& conninfo);

    /**
     * @brief Закрыть соединение.
     */
    void close();

    /**
     * @brief Выполнить SQL без возврата данных (INSERT, UPDATE, DELETE, CREATE).
     * @param sql Запрос.
     * @return true при успехе.
     */
    bool exec(const std::string& sql);

    /**
     * @brief Выполнить SELECT и вернуть результат.
     * @param sql Запрос.
     * @return Указатель на PGresult (необходимо освободить через PQclear).
     */
    PGresult* query(const std::string& sql);

    /**
     * @brief Получить сырой указатель PGconn.
     */
    PGconn* getHandle() const { return conn; }

private:
    PGconn* conn;
};