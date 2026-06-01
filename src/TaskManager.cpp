#include "TaskManager.h"
#include <sstream>
#include <iostream>
#include <cstring>

TaskManager::TaskManager(Database& db) : db(db) {}

bool TaskManager::initTables() {
    std::string sql = R"(
        CREATE TABLE IF NOT EXISTS tasks (
            id SERIAL PRIMARY KEY,
            user_id INTEGER NOT NULL,
            title TEXT NOT NULL,
            priority INTEGER NOT NULL,
            status INTEGER NOT NULL,
            deadline TIMESTAMP,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
        );
        CREATE TABLE IF NOT EXISTS subtasks (
            id SERIAL PRIMARY KEY,
            task_id INTEGER NOT NULL REFERENCES tasks(id) ON DELETE CASCADE,
            title TEXT NOT NULL,
            status INTEGER NOT NULL,
            deadline TIMESTAMP,
            order_num INTEGER DEFAULT 0,
            parent_subtask_id INTEGER REFERENCES subtasks(id) ON DELETE CASCADE
        );
    )";
    return db.exec(sql);
}

bool TaskManager::addTask(int userId, const std::string& title, Priority priority, const std::optional<std::string>& deadline) {
    std::stringstream sql;
    sql << "INSERT INTO tasks (user_id, title, priority, status, deadline) VALUES ("
        << userId << ", '" << escapeSqlString(title) << "', "
        << static_cast<int>(priority) << ", "
        << static_cast<int>(TaskStatus::PENDING) << ", ";
    if (deadline) {
        sql << "'" << *deadline << "'::timestamp)";
    } else {
        sql << "NULL)";
    }
    return db.exec(sql.str());
}

bool TaskManager::updateTask(int userId, int taskId, const std::string& title, Priority priority, const std::optional<std::string>& deadline) {
    std::stringstream sql;
    sql << "UPDATE tasks SET title = '" << escapeSqlString(title) << "', priority = " << static_cast<int>(priority)
        << ", deadline = ";
    if (deadline) {
        sql << "'" << *deadline << "'::timestamp";
    } else {
        sql << "NULL";
    }
    sql << " WHERE id = " << taskId << " AND user_id = " << userId;
    return db.exec(sql.str());
}

bool TaskManager::updateTaskStatus(int userId, int taskId, TaskStatus newStatus) {
    std::stringstream sql;
    sql << "UPDATE tasks SET status = " << static_cast<int>(newStatus)
        << " WHERE id = " << taskId << " AND user_id = " << userId;
    return db.exec(sql.str());
}

bool TaskManager::deleteTask(int userId, int taskId) {
    std::stringstream sql;
    sql << "DELETE FROM tasks WHERE id = " << taskId << " AND user_id = " << userId;
    return db.exec(sql.str());
}

std::vector<Task> TaskManager::getAllTasks(int userId, const std::string& search, const std::string& sort) {
    std::vector<Task> tasks;
    std::stringstream sql;
    sql << "SELECT id, title, priority, status, deadline FROM tasks WHERE user_id = " << userId;
    if (!search.empty()) {
        sql << " AND title ILIKE '%" << escapeSqlString(search) << "%'";
    }
    if (sort == "deadline_asc") {
        sql << " ORDER BY CASE WHEN deadline IS NULL THEN 1 ELSE 0 END, deadline ASC, created_at DESC";
    } else if (sort == "deadline_desc") {
        sql << " ORDER BY CASE WHEN deadline IS NULL THEN 1 ELSE 0 END, deadline DESC, created_at DESC";
    } else if (sort == "priority_desc") {
        sql << " ORDER BY priority DESC, created_at DESC";
    } else if (sort == "priority_asc") {
        sql << " ORDER BY priority ASC, created_at DESC";
    } else if (sort == "status_asc") {
        sql << " ORDER BY status ASC, created_at DESC";
    } else {
        sql << " ORDER BY created_at DESC";
    }
    PGresult* res = db.query(sql.str());
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        std::cerr << "SELECT error: " << PQerrorMessage(db.getHandle()) << std::endl;
        PQclear(res);
        return tasks;
    }
    int rows = PQntuples(res);
    for (int i = 0; i < rows; ++i) {
        Task t;
        t.id = std::stoi(PQgetvalue(res, i, 0));
        t.title = PQgetvalue(res, i, 1);
        t.priority = static_cast<Priority>(std::stoi(PQgetvalue(res, i, 2)));
        t.status = static_cast<TaskStatus>(std::stoi(PQgetvalue(res, i, 3)));
        const char* dl = PQgetvalue(res, i, 4);
        if (dl && strlen(dl) > 0) t.deadline = dl;
        tasks.push_back(t);
    }
    PQclear(res);
    for (auto& t : tasks) {
        t.subtasks = getSubTasksForTask(t.id);
    }
    return tasks;
}

Task TaskManager::getTaskById(int userId, int taskId) {
    Task t;
    std::stringstream sql;
    sql << "SELECT id, title, priority, status, deadline FROM tasks WHERE id = " << taskId
        << " AND user_id = " << userId;
    PGresult* res = db.query(sql.str());
    if (PQntuples(res) > 0) {
        t.id = std::stoi(PQgetvalue(res, 0, 0));
        t.title = PQgetvalue(res, 0, 1);
        t.priority = static_cast<Priority>(std::stoi(PQgetvalue(res, 0, 2)));
        t.status = static_cast<TaskStatus>(std::stoi(PQgetvalue(res, 0, 3)));
        const char* dl = PQgetvalue(res, 0, 4);
        if (dl && strlen(dl)) t.deadline = dl;
    }
    PQclear(res);
    t.subtasks = getSubTasksForTask(taskId);
    return t;
}

bool TaskManager::addSubTask(int userId, int parentTaskId, const std::string& title, const std::optional<std::string>& deadline, int order) {
    std::stringstream check;
    check << "SELECT id FROM tasks WHERE id = " << parentTaskId << " AND user_id = " << userId;
    PGresult* resCheck = db.query(check.str());
    if (PQntuples(resCheck) == 0) {
        PQclear(resCheck);
        return false;
    }
    PQclear(resCheck);
    std::stringstream sql;
    sql << "INSERT INTO subtasks (task_id, title, status, deadline, order_num) VALUES ("
        << parentTaskId << ", '" << escapeSqlString(title) << "', "
        << static_cast<int>(TaskStatus::PENDING) << ", ";
    if (deadline) {
        sql << "'" << *deadline << "'::timestamp, " << order << ")";
    } else {
        sql << "NULL, " << order << ")";
    }
    return db.exec(sql.str());
}

bool TaskManager::updateSubTaskStatus(int userId, int subTaskId, TaskStatus newStatus) {
    std::stringstream sql;
    sql << "UPDATE subtasks SET status = " << static_cast<int>(newStatus)
        << " WHERE id = " << subTaskId
        << " AND task_id IN (SELECT id FROM tasks WHERE user_id = " << userId << ")";
    return db.exec(sql.str());
}

bool TaskManager::deleteSubTask(int userId, int subTaskId) {
    std::stringstream sql;
    sql << "DELETE FROM subtasks WHERE id = " << subTaskId
        << " AND task_id IN (SELECT id FROM tasks WHERE user_id = " << userId << ")";
    return db.exec(sql.str());
}

std::vector<SubTask> TaskManager::getSubTasksForTask(int taskId) {
    std::vector<SubTask> subtasks;
    std::stringstream sql;
    sql << "SELECT id, title, status, deadline, order_num, parent_subtask_id FROM subtasks WHERE task_id = " << taskId
        << " ORDER BY order_num;";
    PGresult* res = db.query(sql.str());
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        return subtasks;
    }
    int rows = PQntuples(res);
    for (int i = 0; i < rows; ++i) {
        SubTask st;
        st.id = std::stoi(PQgetvalue(res, i, 0));
        st.title = PQgetvalue(res, i, 1);
        st.status = static_cast<TaskStatus>(std::stoi(PQgetvalue(res, i, 2)));
        const char* dl = PQgetvalue(res, i, 3);
        if (dl && strlen(dl)) st.deadline = dl;
        st.order = std::stoi(PQgetvalue(res, i, 4));
        const char* parent = PQgetvalue(res, i, 5);
        if (parent && strlen(parent)) st.parent_subtask_id = std::stoi(parent);
        subtasks.push_back(st);
    }
    PQclear(res);
    return subtasks;
}

std::string TaskManager::escapeSqlString(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '\'') out += "''";
        else out += c;
    }
    return out;
}