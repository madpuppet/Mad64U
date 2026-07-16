#pragma once

#include "Singleton.h"
#include "WindowBase.h"
#include "SourceFile.h"
#include <set>
#include <mutex>
#include <map>

class SourceFileManager : public Singleton<SourceFileManager>
{
public:
    SourceFileManager();

    void SaveAll();
    bool NewFile(const std::string &name);
    bool SaveFile(SourceFile* file);
    bool CloseFile(SourceFile* file);
    bool RenameFile(SourceFile* file, const std::string &path);

    SourceFile* GetFileFromWindow(WindowBase* window);

    std::vector<class SourceFile*> m_sourceFiles;
    std::vector<class SourceFileWindow*> m_sourceFileRenderers;

    bool IsKeyword(SourceType sourceType, const char* keyword)
    {
        if (sourceType == SourceType::Asm)
            return m_keywords[(int)SourceType::Asm].contains(HashU8NoCase(0,keyword));
        else
            return m_keywords[(int)sourceType].contains(HashU8(0, keyword));
    }

    void RequestLoadFiles(std::vector<std::string> paths);
    void Tick();

    void RestoreFilesFromSettings();
    void SaveFilesToSettings();

    void LoadRequestedFiles(bool addWindow);
    SourceFile* FindFile(const std::string& path);

protected:
    std::mutex m_lock;
    std::vector<std::string> m_filesToLoad;

    void InitKeywords(const char** keywords, SourceType sourceType, bool caseSensitive);
    std::set<size_t> m_keywords[NumSourceType];
};
