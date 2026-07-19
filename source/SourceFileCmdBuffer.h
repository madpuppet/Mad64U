#pragma once
#include <ranges>
#include <sstream>

class SourceFileCmd
{
public:
    SourceFileCmd(const Vec2i& oldCursor, const Vec2i& newCursor) : m_oldCursor(oldCursor), m_newCursor(newCursor) {}

    Vec2i Execute(class SourceFile* file)
    {
        DoExecute(file);
        return m_newCursor;
    }
    Vec2i Revert(class SourceFile* file)
    {
        DoRevert(file);
        return m_oldCursor;
    }
    virtual std::string Desc() = 0;

    // do actual file work
    virtual void DoExecute(class SourceFile* file) = 0;
    virtual void DoRevert(class SourceFile* file) = 0;

protected:
    Vec2i m_oldCursor;
    Vec2i m_newCursor;
};

class SFC_Group : public SourceFileCmd
{
public:
    SFC_Group(const std::string& desc, const Vec2i& oldCursor, const Vec2i& newCursor) : m_desc(desc), SourceFileCmd(oldCursor, newCursor) {}

    ~SFC_Group()
    {
        for (auto cmd : m_cmds)
            delete cmd;
    }
    void DoExecute(class SourceFile* file)
    {
        for (auto cmd : m_cmds)
            cmd->DoExecute(file);
    }
    void DoRevert(class SourceFile* file)
    {
        for (auto cmd : std::views::reverse(m_cmds))
            cmd->DoRevert(file);
    }
    std::string Desc() override 
    {
        if (m_desc.empty())
        {
            std::ostringstream oss;
            for (auto cmd : m_cmds)
            {
                oss << cmd->Desc() << " ";
            }
            return oss.str();
        }
        else
        {
            return m_desc;
        }
    }
    
    std::string m_desc;
    std::vector<SourceFileCmd*> m_cmds;
};

class SourceFileCmdBuffer
{
public:
    ~SourceFileCmdBuffer();

    std::vector<SourceFileCmd*> m_commandList;
    int m_cmdIndex = 0;

    void PushAndExecute(class SourceFile *file, SourceFileCmd* cmd);
    bool Execute(class SourceFile* file, Vec2i &cursor);
    bool Revert(class SourceFile* file, Vec2i& cursor);
    void Clear();
};


