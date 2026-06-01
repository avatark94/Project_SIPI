/**
 * @file Task.h
 * @brief Модели задачи и подзадачи.
 */
#pragma once

#include <string>
#include <optional>
#include <vector>

enum class Priority { HIGH, MEDIUM, LOW };
enum class TaskStatus { PENDING, COMPLETED, OVERDUE };

struct SubTask {
    int id;
    std::string title;
    std::optional<std::string> description;
    TaskStatus status;
    std::optional<std::string> deadline; // ISO8601
    int order;
    int parent_task_id;
    std::optional<int> parent_subtask_id;

    SubTask() : id(0), status(TaskStatus::PENDING), order(0), parent_task_id(0) {}
};

struct Task {
    int id;
    std::string title;
    std::optional<std::string> description;
    TaskStatus status;
    Priority priority;
    std::optional<std::string> deadline;
    std::vector<SubTask> subtasks;

    Task() : id(0), status(TaskStatus::PENDING), priority(Priority::MEDIUM) {}
};