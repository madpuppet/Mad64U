#pragma once

#include "Singleton.h"
#include "WindowBase.h"
#include "SourceFile.h"
#include <set>
#include <mutex>
#include <map>
#include <filesystem>

struct DisassemblyLine
{
    u32 m_addressStart = 0;
    u32 m_addressLength = 0;
};

struct DisassemblyFile
{
    std::filesystem::path m_path;
    std::vector<DisassemblyLine> m_lines;
    struct DisassemblyInfo* m_info;
};

struct DisassemblyInfo
{
    std::filesystem::path m_dbgPath;
    std::vector<DisassemblyFile*> m_files;
    u8* m_memory;
    u32 m_memoryAddress;
    u32 m_memoryLength;
};

DisassemblyInfo* LoadDisassembly(const std::filesystem::path& path);


class SourceFileManager : public Singleton<SourceFileManager>
{
public:
    SourceFileManager();

    void SaveAll();
    bool NewFile(const std::string &name);
    bool SaveFile(SourceFile* file);
    bool CloseFile(SourceFile* file);
    bool RenameFile(SourceFile* file, const std::string &path);

    std::vector<class SourceFile*> m_sourceFiles;

    bool IsKeyword(SourceType sourceType, const char* keyword)
    {
        if (sourceType == SourceType::Asm)
            return m_keywords[(int)SourceType::Asm].contains(HashU8NoCase(0,keyword));
        else
            return m_keywords[(int)sourceType].contains(HashU8(0, keyword));
    }

    void RequestLoadFiles(std::vector<std::string> paths);

    void RestoreFilesFromSettings();
    void SaveFilesToSettings();

    void LoadRequestedFiles(bool addWindow);
    SourceFile* FindFile(const std::string& path);

    void SetActiveSourceFile(class SourceFile* file);
    class SourceFile* GetActiveSourceFile() { return m_activeSourceFile; }

    void Compile(class SourceFile* file);
    void Run(class SourceFile* file);
    void Run(const std::filesystem::path &outputFile);
    void Deploy(const std::filesystem::path& outputFile);

    void AddDisassembly(DisassemblyInfo* info);
    void UpdateDisassemblies();
    DisassemblyFile *GetDisassembly(class SourceFile* file);

protected:
    std::mutex m_lock;
    std::vector<std::string> m_filesToLoad;
    class SourceFile* m_activeSourceFile = nullptr;

    void InitKeywords(const char** keywords, SourceType sourceType, bool caseSensitive);
    std::set<size_t> m_keywords[NumSourceType];

    std::mutex m_lockDisassembly;
    std::vector<DisassemblyInfo*> m_activeDisassemblies;
    std::vector<DisassemblyInfo*> m_queuedForAddDisassemblies;
};
