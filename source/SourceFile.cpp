#include "common.h"
#include "SourceFile.h"
#include "SourceFileCmdBuffer.h"
#include <filesystem>

SourceFile::SourceFile(const std::string& path) : m_path(path)
{
    m_cmdBuffer = new SourceFileCmdBuffer;

    std::filesystem::path p = path;
    std::filesystem::path ext = p.extension();
    if (ext == ".asm")
        m_sourceType = SourceType::Asm;
    else if (ext == ".c")
        m_sourceType = SourceType::C;
    else
        m_sourceType = SourceType::Text;
}

SourceFile::~SourceFile()
{
    for (auto line : m_lines)
        delete line;
    delete m_cmdBuffer;
}
