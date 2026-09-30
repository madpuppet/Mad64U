#pragma once

enum class SourceType
{
    Text,
    C,
    Asm,
    S,
    MAX
};
constexpr size_t NumSourceType = static_cast<size_t>(SourceType::MAX);

enum class FragmentType
{
    General,
    Operator,
    String,
    Label,
    Comment
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
    int m_uniqueID = 0;
    int m_assembledLine = -1;
    std::string m_chars;
    bool m_fragmentsDirty = true;
    int m_breakpointID = 0;
    std::vector<SourceLineRenderFragment> m_fragments;

    void BuildFragments(SDL_Renderer *renderer, SourceType sourceType);
    int CharIndexToColumn(int index);
};

class SourceFile
{
public:
    SourceFile(const std::string& path);
    ~SourceFile();

    // unique file ID - used for tracking files by a handle
    int m_uniqueLineID = 0;
    int m_fileID;

    std::vector<SourceLine*> m_lines;
    std::string m_path;
    SourceType m_sourceType = SourceType::Asm;
    Recti m_fragmentArea;
    class SourceFileCmdBuffer *m_cmdBuffer;

    // putting in a temporary breakpoint
    int m_stepOverAddr = -1;
    int m_stepOverBreakpointID = 0;
};


