#include "common.h"
#include "SourceFileManager.h"
#include "WindowManager.h"
#include "FontRenderer.h"
#include "Application.h"
#include "SourceFileWindow.h"
#include "SourceFileCmdBuffer.h"
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include "Settings.h"

const char* s_keywords_asm[] = { "tax", "eor", "dec", "pla", "rts", "rti", "bcc", "txa", "clc", "sec", "cpx", "cpy", "cmp", "bne", "beq", "bmi", "bpl", "ldx", "ldy", "stx", "sty", "jsr", "jmp", "tay", "tya", "pha", "dey", "dex", "inc", "inx", "iny", "lda", "sta", "adc", "lsr", "asr", "asl", "lsl", "and", "ora", "xor", "sei", "cli", 0};
const char* s_keywords_c[] = { "void", "for", "if", "else", 0 };

void SourceFileManager::InitKeywords(const char** keywords, SourceType sourceType, bool caseSensitive)
{
    const char** k = keywords;
    while (*k)
    {
        if (caseSensitive)
            m_keywords[(int)SourceType::Asm].insert(HashU8(0, *k));
        else
            m_keywords[(int)SourceType::Asm].insert(HashU8NoCase(0, *k));
        k++;
    }
}

SourceFileManager::SourceFileManager()
{
    InitKeywords(s_keywords_asm, SourceType::Asm, false);
    InitKeywords(s_keywords_c, SourceType::C, true);
}

bool SourceFileManager::RenameFile(SourceFile* file, const std::string& path)
{
    file->m_path = path;

    std::filesystem::path fspath = path;
    std::string name = fspath.filename().string();

    WindowMessageStruct msg;
    msg.m_type = WindowMessage::File_Renamed;
    msg.m_sourceFile = file;
    WindowManager::Instance().MessageAllWindows(msg);

    WindowManager::Instance().LayoutWindows();
    return true;
}

bool SourceFileManager::NewFile(const std::string& path)
{
    auto sourceFile = new SourceFile(path);
    m_sourceFiles.push_back(sourceFile);

    auto sourceFileRenderer = new SourceFileWindow(sourceFile);
    WindowManager::Instance().GetActiveWindowLayout()->AddWindow(sourceFileRenderer);
    WindowManager::Instance().GetActiveWindowTree()->LayoutWindows();
    return true;
}

void SourceFileManager::RequestLoadFiles(std::vector<std::string> paths)
{
    m_lock.lock();
    m_filesToLoad.insert(m_filesToLoad.end(), paths.begin(), paths.end());
    m_lock.unlock();
}

void SourceFileManager::Tick()
{
    LoadRequestedFiles(true);
}

void SourceFileManager::SaveAll()
{
    for (auto file : m_sourceFiles)
    {
        if (!file->m_path.empty())
        {
            SaveFile(file);
        }
    }
}

SourceFile* SourceFileManager::FindFile(const std::string& path)
{
    for (auto file : m_sourceFiles)
    {
        if (file->m_path == path)
            return file;
    }
    return nullptr;
}


void SourceFileManager::LoadRequestedFiles(bool addWindow)
{
    std::vector<std::string> paths;
    m_lock.lock();
    paths = std::move(m_filesToLoad);
    m_lock.unlock();
    if (paths.empty())
        return;

    bool success = false;
    auto& wm = WindowManager::Instance();
    for (auto& path : paths)
    {
        // check if file is already loaded...
        bool exists = false;
        bool needsLayout = false;
        for (auto file : m_sourceFiles)
        {
            if (file->m_path == path)
            {
                // exists... do we need to reopen a window?
                if (addWindow)
                {
                    WindowMessageStruct msg;
                    msg.m_type = WindowMessage::File_Count;
                    msg.m_sourceFile = file;
                    WindowManager::Instance().MessageAllWindows(msg);
                    if (msg.m_count == 0)
                    {
                        auto sourceFileRenderer = new SourceFileWindow(file);
                        wm.AddWindow(sourceFileRenderer);
                        needsLayout = true;
                    }
                }
                exists = true;
                break;
            }
        }

        if (needsLayout)
            wm.LayoutWindows();

        if (exists)
            continue;

        std::ifstream file(path);
        if (!file)
            continue;

        success = true;
        Log("Load %s\n", path.c_str());

        std::string line;
        auto sourceFile = new SourceFile(path);
        while (std::getline(file, line))
        {
            // Strip trailing CR if reading a Windows file on Linux/macOS.
            if (!line.empty() && line.back() == '\r')
            {
                line.pop_back();
            }
            SourceLine* sl = new SourceLine;
            sl->m_chars = std::move(line);
            sourceFile->m_lines.push_back(sl);
        }
        m_sourceFiles.push_back(sourceFile);

        if (addWindow)
        {
            auto sourceFileRenderer = new SourceFileWindow(sourceFile);
            wm.AddWindow(sourceFileRenderer);
        }
    }

    if (addWindow)
    {
        if (success)
            WindowManager::Instance().GetActiveWindowTree()->LayoutWindows();
        WindowManager::Instance().SaveWindowLayout();
    }
}

bool SourceFileManager::SaveFile(SourceFile* file)
{
    if (file->m_path.empty())
        return false;

    std::error_code ec;

    // Create parent directories if they don't exist.
    std::filesystem::path path = file->m_path;
    std::filesystem::create_directories(path.parent_path(), ec);

    std::ofstream fh(path, std::ios::binary);

    if (!fh)
        return false;

    for (const auto &line : file->m_lines)
    {
        fh << line->m_chars << '\n';
    }

    if (fh.good())
        file->m_cmdBuffer->Clear();

    return fh.good();
}

bool SourceFileManager::CloseFile(SourceFile* file)
{
    if (m_activeSourceFile == file)
        m_activeSourceFile = nullptr;

    WindowMessageStruct msg;
    msg.m_type = WindowMessage::File_Deleted;
    msg.m_sourceFile = file;
    WindowManager::Instance().MessageAllWindows(msg);
    WindowManager::Instance().RemoveQueuedWindows();

    // delete the file
    delete file;
    m_sourceFiles.erase(std::find(m_sourceFiles.begin(), m_sourceFiles.end(), file));

    WindowManager::Instance().LayoutWindows();
    WindowManager::Instance().SaveWindowLayout();
    return true;
}

