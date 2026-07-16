#include "common.h"
#include "LogManager.h"

LogManager::LogManager()
{
    m_logs[(int)LogGroup::System].m_name = "System";
    m_logs[(int)LogGroup::Build].m_name = "Build";
}

void LogManager::Log(LogGroup group, const std::string &msg)
{
    m_lock.lock();
    m_logs[(int)group].m_lines.push_back(msg);
    m_lock.unlock();
}

void LogManager::Clear(LogGroup group)
{
    m_lock.lock();
    m_logs[(int)group].m_lines.clear();
    m_lock.unlock();
}
