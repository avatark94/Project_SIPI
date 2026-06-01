#pragma once
#include "Task.h"
#include "Database.h"
#include <vector>
#include <string>

class TaskManager {
public:
    TaskManager(Database& db);
    bool initTables();
    bool addTask(int userId, const std::string& title, Priority priority, const std::optional<std::string>& deadline);
    bool updateTask(int userId, int taskId, const std::string& title, Priority priority, const std::optional<std::string>& deadline);
    bool updateTaskStatus(int userId, int taskId, TaskStatus newStatus);
    bool deleteTask(int userId, int taskId);
    std::vector<Task> getAllTasks(int userId, const std::string& search = "", const std::string& sort = "created_at_desc");
    Task getTaskById(int userId, int taskId);
    bool addSubTask(int userId, int parentTaskId, const std::string& title, const std::optional<std::string>& deadline, int order);
    bool updateSubTaskStatus(int userId, int subTaskId, TaskStatus newStatus);
    bool deleteSubTask(int userId, int subTaskId);
    std::vector<SubTask> getSubTasksForTask(int taskId);

private:
    Database& db;
    std::string escapeSqlString(const std::string& s);
};