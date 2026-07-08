#pragma once

#include "Singleton.h"
#include "WindowBase.h"

enum class FragmentType
{
    General,
    Operator,
    Opcode,
    Operand
};

struct SourceLineRenderFragment
{
    FragmentType m_fragType;
    std::string m_chars;
    Recti m_area;
};

class SourceLine
{
public:
    std::string m_chars;
    std::vector<SourceLineRenderFragment> m_fragments;
};

class SourceFile
{
public:
    SourceFile(const std::string& path) : m_path(path) {}

    std::vector<SourceLine*> m_lines;

    std::string m_path;
    Recti m_fragmentArea;
    int m_cursorRow = 0;
    int m_cursorColumn = 0;
    bool m_modified = false;
};

class SourceFileRenderer : public WindowBase
{
public:
    SourceFileRenderer(SourceFile* file);

    void Paint(SDL_Renderer* renderer, const Recti& dirtyArea) override;
    void Close() override;

    int m_scrollX = 0;
    int m_scrollY = 0;
    SourceFile* m_sourceFile;
};

class SourceFileManager : public Singleton<SourceFileManager>
{
public:
    bool NewFile(const std::string &name);
    bool LoadFile(const std::string& path);
    bool SaveFile(SourceFile* file);
    bool CloseFile(SourceFile* file);
    bool RenameFile(SourceFile* file, const std::string &path);

    SourceFile* GetFileFromWindow(WindowBase* window);

    std::vector<SourceFile*> m_sourceFiles;
    std::vector<SourceFileRenderer*> m_sourceFileRenderers;
};


