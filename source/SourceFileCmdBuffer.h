#pragma once
#include <ranges>

class SourceFileCmd
{
public:
    virtual void Execute(class SourceFile* file) = 0;
    virtual void Revert(class SourceFile* file) = 0;
    virtual std::string Desc() = 0;
};

class SFC_Group : public SourceFileCmd
{
public:
    SFC_Group(const std::string& desc) : m_desc(desc) {}

    ~SFC_Group()
    {
        for (auto cmd : m_cmds)
            delete cmd;
    }
    void Execute(class SourceFile* file)
    {
        for (auto cmd : m_cmds)
            cmd->Execute(file);
    }
    void Revert(class SourceFile* file)
    {
        for (auto cmd : std::views::reverse(m_cmds))
            cmd->Revert(file);
    }
    std::string Desc() override { return m_desc; }
    
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
    void Execute(class SourceFile* file);
    void Revert(class SourceFile* file);
};


