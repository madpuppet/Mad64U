#pragma once

enum class SourceType
{
    Text,
    C,
    Asm,
    MAX
};
constexpr size_t NumSourceType = static_cast<size_t>(SourceType::MAX);

enum class FragmentType
{
    General,
    Operator,
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
    std::string m_chars;
    bool m_fragmentsDirty = true;
    std::vector<SourceLineRenderFragment> m_fragments;
};

class SourceFile
{
public:
    SourceFile(const std::string& path);
    ~SourceFile();

    std::vector<SourceLine*> m_lines;
    std::string m_path;
    SourceType m_sourceType = SourceType::Asm;
    Recti m_fragmentArea;
    class SourceFileCmdBuffer *m_cmdBuffer;
};


