#pragma once

#include "Singleton.h"
#include <mutex>
#include <format>
#include <utility>
#include <array>

enum class LogGroup
{
    System,
    Build,
    MAX
};
constexpr size_t NumLogGroup = static_cast<size_t>(LogGroup::MAX);

struct LogGroupData
{
    std::string m_name;
    std::vector<std::string> m_lines;
};

class LogManager : public Singleton<LogManager>
{
public:
    LogManager();
    void Log(LogGroup group, const std::string &msg);
    void Clear(LogGroup group);

    std::array<LogGroupData, NumLogGroup> m_logs;
    std::mutex m_lock;
};

template<typename... Args>
inline void Log(LogGroup group,
    std::format_string<Args...> fmt,
    Args&&... args)
{
    LogManager::Instance().Log(
        group,
        std::format(fmt, std::forward<Args>(args)...));
}

inline void Log(LogGroup group, std::string str)
{
    LogManager::Instance().Log(group, str);
}
