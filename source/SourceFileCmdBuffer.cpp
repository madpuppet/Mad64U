#include "common.h"
#include "SourceFileCmdBuffer.h"

void SourceFileCmdBuffer::PushAndExecute(class SourceFile* file, SourceFileCmd* cmd)
{
    // clear off any unexecuted cmds
    if (m_cmdIndex < (int)m_commandList.size())
    {
        for (int i = m_cmdIndex; i < m_commandList.size(); i++)
            delete m_commandList[i];
        m_commandList.erase(m_commandList.begin() + m_cmdIndex, m_commandList.end());
    }
    m_commandList.push_back(cmd);
    m_cmdIndex++;
    cmd->Execute(file);
}

bool SourceFileCmdBuffer::Execute(class SourceFile* file, Vec2i& cursor)
{
    if (m_cmdIndex < (int)m_commandList.size())
    {
        cursor = m_commandList[m_cmdIndex++]->Execute(file);
        return true;
    }
    return false;
}

bool SourceFileCmdBuffer::Revert(class SourceFile* file, Vec2i& cursor)
{
    if (m_cmdIndex > 0)
    {
        cursor = m_commandList[--m_cmdIndex]->Revert(file);
        return true;
    }
    return false;
}

SourceFileCmdBuffer::~SourceFileCmdBuffer()
{
    for (auto cmd : m_commandList)
        delete cmd;
}

void SourceFileCmdBuffer::Clear()
{
    for (auto cmd : m_commandList)
        delete cmd;
    m_commandList.clear();
    m_cmdIndex = 0;
}

