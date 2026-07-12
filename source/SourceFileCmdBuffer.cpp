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

void SourceFileCmdBuffer::Execute(class SourceFile* file)
{
    if (m_cmdIndex < (int)m_commandList.size())
    {
        m_commandList[m_cmdIndex++]->Execute(file);
    }
}

void SourceFileCmdBuffer::Revert(class SourceFile* file)
{
    if (m_cmdIndex > 0)
    {
        m_commandList[--m_cmdIndex]->Execute(file);
    }
}

SourceFileCmdBuffer::~SourceFileCmdBuffer()
{
    for (auto cmd : m_commandList)
        delete cmd;
}

