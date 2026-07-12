#include "common.h"
#include "SourceFile.h"
#include "SourceFileCmdBuffer.h"

SourceFile::SourceFile(const std::string& path) : m_path(path)
{
    m_cmdBuffer = new SourceFileCmdBuffer;
}

SourceFile::~SourceFile()
{
    for (auto line : m_lines)
        delete line;
    delete m_cmdBuffer;
}
