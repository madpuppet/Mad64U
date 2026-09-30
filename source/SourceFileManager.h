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
    SourceFile* FindFileByID(int id);

    void SetActiveSourceFile(class SourceFile* file);
    class SourceFile* GetActiveSourceFile() { return m_activeSourceFile; }

    void Compile(class SourceFile* file, bool run);
    void Run(class SourceFile* file);
    void Run(const std::filesystem::path &outputFile);
    void Deploy(const std::filesystem::path& outputFile);

    // load a dbg file 
    // checks that no other loads of this dbg file are in process
    // if checkNotAlreadyLoaded, then it checks if any added disassembly matches, otherwise added disassemblies will get replaced
    // safe to call from threads
    void LoadDisassembly(const std::filesystem::path dbg, bool checkNotAlreadyLoaded);

    // adds any disassemblies that were queued on thread into the active list
    // should be called by main render thread
    void UpdateDisassemblies();

    // gets the disassembly matching this file, if it is active
    DisassemblyFile* GetDisassembly(class SourceFile* file);

    void OnBreakpointSet(int fileID, int lineID, int breakpointID, int addr);

protected:
    bool StartLoadingDisassembly(const std::filesystem::path dbg);
    void FinishLoadingDisassembly(const std::filesystem::path dbg);
    void AddDisassembly(DisassemblyInfo* info);

    std::mutex m_lock;
    std::vector<std::string> m_filesToLoad;
    class SourceFile* m_activeSourceFile = nullptr;

    void InitKeywords(const char** keywords, SourceType sourceType, bool caseSensitive);
    std::set<size_t> m_keywords[NumSourceType];

    std::mutex m_lockDisassembly;
    std::vector<DisassemblyInfo*> m_activeDisassemblies;
    std::vector<DisassemblyInfo*> m_queuedForAddDisassemblies;
    std::vector<std::filesystem::path> m_loadingDisassemblies;
};
