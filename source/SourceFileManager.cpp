#include "common.h"
#include "SourceFileManager.h"
#include "WindowManager.h"
#include "FontRenderer.h"
#include "Application.h"
#include "SourceFileRenderer.h"
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

    for (auto fr : m_sourceFileRenderers)
    {
        if (fr->m_sourceFile == file)
        {
            fr->m_name = name;
        }
    }

    WindowManager::Instance().LayoutWindows();
    return true;
}

bool SourceFileManager::NewFile(const std::string& path)
{
    auto sourceFile = new SourceFile(path);
    m_sourceFiles.push_back(sourceFile);

    auto sourceFileRenderer = new SourceFileRenderer(sourceFile);
    m_sourceFileRenderers.push_back(sourceFileRenderer);
    WindowManager::Instance().GetActiveWindowLayout()->AddWindow(sourceFileRenderer);
    WindowManager::Instance().GetActiveWindowTree()->LayoutWindows();
    return true;
}

SourceFile* SourceFileManager::GetFileFromWindow(WindowBase* window)
{
    for (auto sfr : m_sourceFileRenderers)
    {
        if (sfr == window)
        {
            return sfr->m_sourceFile;
        }
    }
    return nullptr;
}

void SourceFileManager::RequestLoadFiles(std::vector<std::string> paths)
{
    m_lock.lock();
    m_filesToLoad.insert(m_filesToLoad.end(), paths.begin(), paths.end());
    m_lock.unlock();
}

void SourceFileManager::Tick()
{
    LoadRequestedFiles();
}

void SourceFileManager::LoadRequestedFiles()
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
        for (auto file : m_sourceFiles)
        {
            if (file->m_path == path)
            {
                exists = true;
                break;
            }
        }

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

        auto sourceFileRenderer = new SourceFileRenderer(sourceFile);
        m_sourceFileRenderers.push_back(sourceFileRenderer);
        wm.AddWindow(sourceFileRenderer);
    }
    if (success)
        WindowManager::Instance().GetActiveWindowTree()->LayoutWindows();
    SaveFilesToSettings();
}

bool SourceFileManager::SaveFile(SourceFile* file)
{
    return true;
}

bool SourceFileManager::CloseFile(SourceFile* file)
{
    // delete all renderers
    std::vector<SourceFileRenderer*> removed;

    for (auto fr : m_sourceFileRenderers)
    {
        if (fr->m_sourceFile == file)
        {
            removed.push_back(fr);
            WindowManager::Instance().RemoveWindow(fr);
        }
    }

    auto condition = [&removed](const auto& fr) -> bool
        {
            return std::find(removed.begin(), removed.end(), fr) != removed.end();
        };
    std::erase_if(m_sourceFileRenderers, condition);

    // delete the file
    delete file;
    m_sourceFiles.erase(std::find(m_sourceFiles.begin(), m_sourceFiles.end(), file));

    WindowManager::Instance().LayoutWindows();
    SaveFilesToSettings();
    return true;
}

void SourceFileManager::RestoreFilesFromSettings()
{
    auto fileList = Settings::Instance().GetStringList(SETTING_FILES);
    RequestLoadFiles(fileList);
}

void SourceFileManager::SaveFilesToSettings()
{
    std::vector<std::string> paths;
    for (auto file : m_sourceFiles)
    {
        paths.push_back(file->m_path);
    }
    Settings::Instance().SetStringList(SETTING_FILES, paths);
    Settings::Instance().Save();
}
