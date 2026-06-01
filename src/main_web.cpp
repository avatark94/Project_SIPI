/**
 * @file main_web.cpp
 * @brief Главный модуль веб-сервера приложения «Планировщик задач».
 *
 * Реализует HTTP-сервер на основе библиотеки cpp-httplib,
 * обрабатывает REST API для задач, подзадач, аутентификации,
 * а также раздаёт статические файлы (HTML, CSS, JS).
 */

#include "httplib.h"
#include "json.hpp"
#include "TaskManager.h"
#include "UserManager.h"
#include "JwtUtils.h"
#include "Database.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstdlib>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#include <libgen.h>
#include <cstring>
#endif

using json = nlohmann::json;
using namespace httplib;

// ========================= Глобальные объекты =========================

/** @brief Глобальный объект для работы с базой данных. */
Database db;

/** @brief Указатель на менеджер задач (CRUD, подзадачи). */
TaskManager* taskManager = nullptr;

/** @brief Указатель на менеджер пользователей (регистрация, аутентификация). */
UserManager* userManager = nullptr;

// ==================== Вспомогательные функции ====================

#ifdef __APPLE__
/**
 * @brief Получить путь к каталогу, содержащему исполняемый файл (macOS).
 * @return Строка с путём (например, "/app" или "." при ошибке).
 */
std::string getExecutablePath() {
    char path[1024];
    uint32_t size = sizeof(path);
    if (_NSGetExecutablePath(path, &size) == 0) {
        return std::string(dirname(path));
    }
    return ".";
}
#else
/**
 * @brief Получить путь к исполняемому файлу (Linux / Docker).
 * @return Всегда возвращает "." (текущая рабочая директория).
 */
std::string getExecutablePath() {
    return ".";
}
#endif

/**
 * @brief Прочитать содержимое текстового файла.
 * @param path Полный путь к файлу.
 * @return Содержимое файла в виде строки, либо пустая строка, если файл не открыт.
 */
std::string readFile(const std::string& path) {
    std::ifstream t(path);
    if (!t.is_open()) return "";
    std::stringstream buffer;
    buffer << t.rdbuf();
    return buffer.str();
}

/**
 * @brief Преобразовать объект Task в JSON-объект.
 * @param t Задача.
 * @return JSON-представление задачи (id, title, status, priority, deadline, subtasks).
 */
json taskToJson(const Task& t) {
    json j;
    j["id"] = t.id;
    j["title"] = t.title;
    j["status"] = (t.status == TaskStatus::PENDING) ? "pending" :
                  (t.status == TaskStatus::COMPLETED) ? "completed" : "overdue";
    j["priority"] = (t.priority == Priority::HIGH) ? "high" :
                    (t.priority == Priority::MEDIUM) ? "medium" : "low";
    if (t.deadline) j["deadline"] = *t.deadline;
    json subs = json::array();
    for (const auto& st : t.subtasks) {
        json sj;
        sj["id"] = st.id;
        sj["title"] = st.title;
        sj["status"] = (st.status == TaskStatus::PENDING) ? "pending" : "completed";
        if (st.deadline) sj["deadline"] = *st.deadline;
        sj["order"] = st.order;
        subs.push_back(sj);
    }
    j["subtasks"] = subs;
    return j;
}

/**
 * @brief Извлечь ID пользователя из JWT-токена в заголовке Authorization.
 * @param req HTTP-запрос.
 * @return ID пользователя (положительное число) или -1, если токен недействителен или отсутствует.
 */
int getUserIdFromRequest(const Request& req) {
    if (!req.has_header("Authorization")) return -1;
    std::string auth = req.get_header_value("Authorization");
    const std::string prefix = "Bearer ";
    if (auth.size() > prefix.size() && auth.substr(0, prefix.size()) == prefix) {
        std::string token = auth.substr(prefix.size());
        return JwtUtils::verifyToken(token);
    }
    return -1;
}

// ========================= Точка входа =========================

/**
 * @brief Главная функция приложения.
 *
 * Инициализирует базу данных, создаёт необходимые таблицы,
 * настраивает маршруты HTTP-сервера и запускает его.
 * @return 0 при успешном завершении, иначе код ошибки.
 */
int main() {
    // --- Подключение к базе данных ---
    const char* db_conn_str = std::getenv("DATABASE_URL");
    if (!db_conn_str) {
        db_conn_str = "host=localhost dbname=taskplanner user=taskuser password=taskpass";
        std::cout << "Using local database configuration" << std::endl;
    }
    if (!db.open(db_conn_str)) {
        std::cerr << "Cannot connect to DB" << std::endl;
        return 1;
    }
    taskManager = new TaskManager(db);
    userManager = new UserManager(db);
    if (!userManager->initTables() || !taskManager->initTables()) {
        std::cerr << "Failed to init tables" << std::endl;
        return 1;
    }

    Server svr;   ///< HTTP-сервер

    // --- Статические файлы (frontend) ---
    std::string exePath = getExecutablePath();
    std::string publicPath = exePath + "/public/";
    svr.Get("/", [publicPath](const Request& req, Response& res) {
        std::string html = readFile(publicPath + "index.html");
        res.set_content(html, "text/html; charset=utf-8");
    });
    svr.Get("/style.css", [publicPath](const Request& req, Response& res) {
        std::string css = readFile(publicPath + "style.css");
        res.set_content(css, "text/css; charset=utf-8");
    });
    svr.Get("/script.js", [publicPath](const Request& req, Response& res) {
        std::string js = readFile(publicPath + "script.js");
        res.set_content(js, "application/javascript; charset=utf-8");
    });

    // --- Маршруты аутентификации ---

    /**
     * @brief Регистрация нового пользователя.
     * @endpoint POST /auth/register
     * @param email, password, name (опционально) в теле JSON.
     * @return JSON { "status": "ok", "user_id": id } или ошибка.
     */
    svr.Post("/auth/register", [](const Request& req, Response& res) {
        try {
            auto j = json::parse(req.body);
            std::string email = j.value("email", "");
            std::string password = j.value("password", "");
            std::optional<std::string> name;
            if (j.contains("name") && !j["name"].is_null()) name = j["name"].get<std::string>();
            if (email.empty() || password.empty()) throw std::runtime_error("Email and password required");
            auto userId = userManager->registerUser(email, password, name);
            if (userId) {
                res.set_content("{\"status\":\"ok\", \"user_id\":" + std::to_string(*userId) + "}", "application/json");
            } else {
                res.status = 400;
                res.set_content("{\"error\":\"User already exists\"}", "application/json");
            }
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content("{\"error\":\"" + std::string(e.what()) + "\"}", "application/json");
        }
    });

    /**
     * @brief Аутентификация и выдача JWT-токена.
     * @endpoint POST /auth/login
     * @param email, password в теле JSON.
     * @return JSON { "access_token": "...", "token_type": "bearer" } или ошибка.
     */
    svr.Post("/auth/login", [](const Request& req, Response& res) {
        try {
            auto j = json::parse(req.body);
            std::string email = j.value("email", "");
            std::string password = j.value("password", "");
            auto userId = userManager->authenticateUser(email, password);
            if (userId) {
                std::string token = JwtUtils::generateToken(*userId);
                json resp;
                resp["access_token"] = token;
                resp["token_type"] = "bearer";
                res.set_content(resp.dump(), "application/json");
            } else {
                res.status = 401;
                res.set_content("{\"error\":\"Invalid credentials\"}", "application/json");
            }
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content("{\"error\":\"" + std::string(e.what()) + "\"}", "application/json");
        }
    });

    // --- API для задач ---

    /**
     * @brief Получить список задач пользователя (с фильтрацией и сортировкой).
     * @endpoint GET /api/tasks?search=&sort=
     * @param search (опционально) – поиск по названию.
     * @param sort (опционально) – вариант сортировки.
     * @return JSON-массив задач.
     */
    svr.Get("/api/tasks", [](const Request& req, Response& res) {
        int userId = getUserIdFromRequest(req);
        if (userId == -1) {
            res.status = 401;
            res.set_content("{\"error\":\"Unauthorized\"}", "application/json");
            return;
        }
        std::string search = req.has_param("search") ? req.get_param_value("search") : "";
        std::string sort = req.has_param("sort") ? req.get_param_value("sort") : "created_at_desc";
        auto tasks = taskManager->getAllTasks(userId, search, sort);
        json result = json::array();
        for (const auto& t : tasks) result.push_back(taskToJson(t));
        res.set_content(result.dump(), "application/json");
    });

    /**
     * @brief Получить одну задачу по ID.
     * @endpoint GET /api/tasks/{id}
     * @return JSON задачи или 404.
     */
    svr.Get(R"(/api/tasks/(\d+))", [](const Request& req, Response& res) {
        int userId = getUserIdFromRequest(req);
        if (userId == -1) {
            res.status = 401;
            res.set_content("{\"error\":\"Unauthorized\"}", "application/json");
            return;
        }
        int taskId = std::stoi(req.matches[1]);
        Task task = taskManager->getTaskById(userId, taskId);
        if (task.id == 0) {
            res.status = 404;
            res.set_content("{\"error\":\"Task not found\"}", "application/json");
            return;
        }
        res.set_content(taskToJson(task).dump(), "application/json");
    });

    /**
     * @brief Создать новую задачу.
     * @endpoint POST /api/tasks
     * @body JSON { title, priority, deadline }.
     * @return 201 Created при успехе.
     */
    svr.Post("/api/tasks", [](const Request& req, Response& res) {
        int userId = getUserIdFromRequest(req);
        if (userId == -1) {
            res.status = 401;
            res.set_content("{\"error\":\"Unauthorized\"}", "application/json");
            return;
        }
        try {
            auto j = json::parse(req.body);
            std::string title = j.value("title", "");
            if (title.empty()) throw std::runtime_error("Title required");
            std::string prioStr = j.value("priority", "medium");
            Priority p = (prioStr == "high") ? Priority::HIGH :
                         (prioStr == "low") ? Priority::LOW : Priority::MEDIUM;
            std::optional<std::string> deadline;
            if (j.contains("deadline") && !j["deadline"].is_null()) deadline = j["deadline"].get<std::string>();
            if (taskManager->addTask(userId, title, p, deadline)) {
                res.status = 201;
                res.set_content("{\"status\":\"ok\"}", "application/json");
            } else {
                res.status = 500;
                res.set_content("{\"error\":\"DB error\"}", "application/json");
            }
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content("{\"error\":\"" + std::string(e.what()) + "\"}", "application/json");
        }
    });

    /**
     * @brief Обновить статус задачи (выполнена / активна).
     * @endpoint PUT /api/tasks/{id}/status
     * @body JSON { status: "completed" | "pending" }.
     */
    svr.Put(R"(/api/tasks/(\d+)/status)", [](const Request& req, Response& res) {
        int userId = getUserIdFromRequest(req);
        if (userId == -1) {
            res.status = 401;
            res.set_content("{\"error\":\"Unauthorized\"}", "application/json");
            return;
        }
        int taskId = std::stoi(req.matches[1]);
        try {
            auto j = json::parse(req.body);
            std::string status = j.value("status", "");
            TaskStatus newStatus = (status == "completed") ? TaskStatus::COMPLETED : TaskStatus::PENDING;
            if (taskManager->updateTaskStatus(userId, taskId, newStatus)) {
                res.set_content("{\"status\":\"ok\"}", "application/json");
            } else {
                res.status = 500;
                res.set_content("{\"error\":\"Update failed\"}", "application/json");
            }
        } catch (...) {
            res.status = 400;
            res.set_content("{\"error\":\"Invalid JSON\"}", "application/json");
        }
    });

    /**
     * @brief Полное обновление задачи (название, приоритет, дедлайн).
     * @endpoint PUT /api/tasks/{id}
     * @body JSON { title, priority, deadline }.
     */
    svr.Put(R"(/api/tasks/(\d+))", [](const Request& req, Response& res) {
        int userId = getUserIdFromRequest(req);
        if (userId == -1) {
            res.status = 401;
            res.set_content("{\"error\":\"Unauthorized\"}", "application/json");
            return;
        }
        int taskId = std::stoi(req.matches[1]);
        try {
            auto j = json::parse(req.body);
            std::string title = j.value("title", "");
            if (title.empty()) throw std::runtime_error("Title required");
            std::string priorityStr = j.value("priority", "medium");
            Priority p = (priorityStr == "high") ? Priority::HIGH :
                         (priorityStr == "low") ? Priority::LOW : Priority::MEDIUM;
            std::optional<std::string> deadline;
            if (j.contains("deadline") && !j["deadline"].is_null()) deadline = j["deadline"].get<std::string>();
            if (taskManager->updateTask(userId, taskId, title, p, deadline)) {
                res.set_content("{\"status\":\"ok\"}", "application/json");
            } else {
                res.status = 500;
                res.set_content("{\"error\":\"Update failed\"}", "application/json");
            }
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content("{\"error\":\"" + std::string(e.what()) + "\"}", "application/json");
        }
    });

    /**
     * @brief Удалить задачу (каскадно удаляются подзадачи и напоминания).
     * @endpoint DELETE /api/tasks/{id}
     */
    svr.Delete(R"(/api/tasks/(\d+))", [](const Request& req, Response& res) {
        int userId = getUserIdFromRequest(req);
        if (userId == -1) {
            res.status = 401;
            res.set_content("{\"error\":\"Unauthorized\"}", "application/json");
            return;
        }
        int taskId = std::stoi(req.matches[1]);
        if (taskManager->deleteTask(userId, taskId)) {
            res.set_content("{\"status\":\"ok\"}", "application/json");
        } else {
            res.status = 500;
            res.set_content("{\"error\":\"Delete failed\"}", "application/json");
        }
    });

    // --- API для подзадач ---

    /**
     * @brief Добавить подзадачу к задаче.
     * @endpoint POST /api/tasks/{taskId}/subtasks
     * @body JSON { title, order, deadline }.
     */
    svr.Post(R"(/api/tasks/(\d+)/subtasks)", [](const Request& req, Response& res) {
        int userId = getUserIdFromRequest(req);
        if (userId == -1) {
            res.status = 401;
            res.set_content("{\"error\":\"Unauthorized\"}", "application/json");
            return;
        }
        int taskId = std::stoi(req.matches[1]);
        try {
            auto j = json::parse(req.body);
            std::string title = j.value("title", "");
            if (title.empty()) throw std::runtime_error("Title required");
            int order = j.value("order", 0);
            std::optional<std::string> deadline;
            if (j.contains("deadline") && !j["deadline"].is_null()) deadline = j["deadline"].get<std::string>();
            if (taskManager->addSubTask(userId, taskId, title, deadline, order)) {
                res.status = 201;
                res.set_content("{\"status\":\"ok\"}", "application/json");
            } else {
                res.status = 500;
                res.set_content("{\"error\":\"DB error\"}", "application/json");
            }
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content("{\"error\":\"" + std::string(e.what()) + "\"}", "application/json");
        }
    });

    /**
     * @brief Обновить статус подзадачи.
     * @endpoint PUT /api/subtasks/{id}/status
     * @body JSON { status: "completed" | "pending" }.
     */
    svr.Put(R"(/api/subtasks/(\d+)/status)", [](const Request& req, Response& res) {
        int userId = getUserIdFromRequest(req);
        if (userId == -1) {
            res.status = 401;
            res.set_content("{\"error\":\"Unauthorized\"}", "application/json");
            return;
        }
        int subId = std::stoi(req.matches[1]);
        try {
            auto j = json::parse(req.body);
            std::string status = j.value("status", "");
            TaskStatus newStatus = (status == "completed") ? TaskStatus::COMPLETED : TaskStatus::PENDING;
            if (taskManager->updateSubTaskStatus(userId, subId, newStatus)) {
                res.set_content("{\"status\":\"ok\"}", "application/json");
            } else {
                res.status = 500;
                res.set_content("{\"error\":\"Update failed\"}", "application/json");
            }
        } catch (...) {
            res.status = 400;
            res.set_content("{\"error\":\"Invalid JSON\"}", "application/json");
        }
    });

    // --- Запуск сервера ---
    std::cout << "Server listening on http://0.0.0.0:8080" << std::endl;
    svr.listen("0.0.0.0", 8080);

    // --- Освобождение ресурсов ---
    delete taskManager;
    delete userManager;
    db.close();
    return 0;
}