#include "common.h"
#include "SourceFileManager.h"
#include "WindowManager.h"
#include "FontRenderer.h"
#include "Application.h"
#include "SourceFileWindow.h"
#include "SourceFileCmdBuffer.h"
#include "NetworkManager.h"
#include "LogManager.h"
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <format>
#include "Settings.h"

const char* s_keywords_asm[] = { "tax", "eor", "dec", "pla", "rts", "rti", "bcc", "bcs", "txa", "clc", "sec",
            "cpx", "cpy", "cmp", "bne", "beq", "bmi", "bpl", "ldx", "ldy", "stx", "sty", "jsr", "jmp", "nop",
            "tay", "tya", "pha", "dey", "dex", "inc", "inx", "iny", "lda", "sta", "adc", "lsr", "asr",
        "asl", "lsl", "and", "ora", "xor", "sei", "cli", "//", ";", ":", ".label", "#import", "#", "$",
    ".word", ".byte", ".import", "binary", "*", "=", ".for", "var", "round", "sin", ",", "%", 0};

const char* s_keywords_s[] = { "tax", "eor", "dec", "pla", "rts", "rti", "bcc", "bcs", "txa", "clc", "sec",
            "cpx", "cpy", "cmp", "bne", "beq", "bmi", "bpl", "ldx", "ldy", "stx", "sty", "jsr", "jmp", "nop",
            "tay", "tya", "pha", "dey", "dex", "inc", "inx", "iny", "lda", "sta", "adc", "lsr", "asr",
        "asl", "lsl", "and", "ora", "xor", "sei", "cli", "//", ";", ":", ".label", "#import", "#", "$",
    ".word", ".byte", ".import", "binary", "*", "=", ".for", "var", "round", "sin", ",", "%",
    ".fopt", ".setcpu", ".smart", ".autoimport", ".debuginfo", ".importzp", ".dbg", ".forceimport", ".export", ".macpack", ".case",
    0 };

const char* s_keywords_c[] = { "char", "int", "long", "short", "(", ")", ";", "{", "}", "[", "]",
            "void", "while", "for", "if", "else", "#define", "#ifdef", "#include", "//",
            "<", ">", "=", "==", "!=", "+", "-", "*", "/", "++", "--", "+=", "-=", 0};

u32 ParseHex(std::string_view text)
{
    u32 value = 0;
    std::from_chars(text.data() + 1, text.data() + text.size(), value, 16);
    return value;
}

void FindC64DebugFiles(std::filesystem::path outPath, std::vector<std::filesystem::path>& files)
{
    const std::filesystem::path outputFolder = outPath.parent_path() / "out";
    std::vector<std::filesystem::path> dbgFiles;
    std::vector<std::filesystem::path> prgFiles;

    if (!std::filesystem::exists(outputFolder) || !std::filesystem::is_directory(outputFolder))
        return;

    for (const std::filesystem::directory_entry& entry :
        std::filesystem::directory_iterator(outputFolder))
    {
        if (!entry.is_regular_file())
            continue;

        std::string extension = entry.path().extension().string();

        std::ranges::transform(
            extension,
            extension.begin(),
            [](unsigned char character)
            {
                return static_cast<char>(std::tolower(character));
            });

        if (extension == ".dbg")
            dbgFiles.push_back(entry.path());
        if (extension == ".prg")
            prgFiles.push_back(entry.path());
    }

    // now we check for every dbg file that has a matching prg file we can return it
    for (auto& dbg : dbgFiles)
    {
        auto out = dbg;
        out.replace_extension(".prg");
        if (std::find(prgFiles.begin(), prgFiles.end(), out) != prgFiles.end())
            files.push_back(dbg);
    }
}

std::vector<std::filesystem::path> FindC64Outputs(std::filesystem::path outPath)
{
    const std::filesystem::path outputFolder = outPath.parent_path() / "out";
    std::vector<std::filesystem::path> files;

    if (!std::filesystem::exists(outputFolder) || !std::filesystem::is_directory(outputFolder))
        return files;

    for (const std::filesystem::directory_entry& entry :
        std::filesystem::directory_iterator(outputFolder))
    {
        if (!entry.is_regular_file())
            continue;

        std::string extension = entry.path().extension().string();

        std::ranges::transform(
            extension,
            extension.begin(),
            [](unsigned char character)
            {
                return static_cast<char>(std::tolower(character));
            });

        if (extension == ".d64" || extension == ".prg" || extension == ".s")
            files.push_back(entry.path());
    }

    return files;
}


bool LoadBinaryFile(const std::filesystem::path& outputFile, u8*& mem, size_t& outputSize)
{
    outputSize = 0;

    std::ifstream file(outputFile, std::ios::binary | std::ios::ate);
    if (!file)
        return false;

    const std::streampos endPosition = file.tellg();
    if (endPosition < 0)
        return false;

    outputSize = (size_t)endPosition;

    mem = new u8[outputSize];
    file.seekg(0, std::ios::beg);

    if (outputSize > 0 && !file.read((char*)mem, outputSize))
        return false;

    return true;
}

class DebugParser
{
public:
    DebugParser(char* mem, u32 size) : m_mem(mem), m_ptr(mem), m_size(size)
    {
        m_end = m_mem + m_size;
    }

    bool PeekToken(std::string& result)
    {
        auto oldPtr = m_ptr;
        if (!PopToken(result))
            return false;
        m_ptr = oldPtr;
        return true;
    }

    bool PopComma()
    {
        std::string token;
        if (!PopToken(token))
            return false;
        return token == ",";
    }

    u32 PopAddr()
    {
        std::string token;
        PopToken(token);
        return ParseHex(token);
    }

    u32 PopU32()
    {
        std::string token;
        PopToken(token);
        return stoi(token);
    }

    bool PopToken(std::string& result)
    {
        result.clear();
        while (m_ptr < m_end)
        {
            // skip empty space
            while (m_ptr < m_end && (*m_ptr == ' ' || *m_ptr == '\t' || *m_ptr == '\n' || *m_ptr == '\r'))
                m_ptr++;

            if (m_ptr[0] == '<' && m_ptr[1] == '/')
            {
                result.push_back(*m_ptr);
                result.push_back(m_ptr[1]);
                m_ptr += 2;
                return true;
            }

            if (*m_ptr == '<' || *m_ptr == '>' || *m_ptr == ',' || *m_ptr == '=')
            {
                result.push_back(*m_ptr);
                m_ptr++;
                return true;
            }

            if (*m_ptr == '"')
            {
                m_ptr++;
                while (m_ptr < m_end && *m_ptr != '"')
                {
                    result.push_back(*m_ptr++);
                }
                m_ptr++;
                return true;
            }

            while ((*m_ptr >= 'a' && *m_ptr <= 'z') || (*m_ptr >= 'A' && *m_ptr <= 'Z') || (*m_ptr >= '0' && *m_ptr <= '9') || *m_ptr == '/' || *m_ptr == '\\' || *m_ptr == '.' || *m_ptr == ':' || *m_ptr == '$' || *m_ptr == '_')
            {
                result.push_back(*m_ptr++);
            }

            if (!result.empty())
                return true;

            m_ptr++;
        }
        return false;
    }

    char* m_mem;
    char* m_ptr;
    char* m_end;
    u32 m_size;
};

class C64DebugParser : public DebugParser
{
public:
    struct DebugLine
    {
        int fileNo;
        int addrStart;
        int addrEnd;
        int line1;
        int col1;
        int line2;
        int col2;
    };
    struct DebugLabel
    {
        std::string name;
        int addr;
        int fileNo;
        int line1;
        int col1;
        int line2;
        int col2;
    };

    C64DebugParser(char* mem, u32 size) : DebugParser(mem, size) {}

    bool PopHeaderStart(std::string& title, std::string &name)
    {
        std::string token;
        PopToken(token);
        if (token != "<")
            return false;
        if (!PopToken(title))
            return false;
        while (PopToken(token))
        {
            if (token == "name")
            {
                PopToken(token);
                PopToken(name);
            }

            if (token == ">")
                return true;
        }
        return false;
    }
    bool PopHeaderEnd(const std::string& title)
    {
        std::string token;
        PopToken(token);
        if (token != "</")
            return false;
        PopToken(token);
        if (token != title)
            return false;
        while (PopToken(token))
        {
            if (token == ">")
                return true;
        }
        return false;
    }

    bool PopSources()
    {
        while (true)
        {
            std::string token;
            if (!PeekToken(token))
                return false;
            if (token == "</")
                break;
            std::string fileNo;
            std::string separator;
            std::string pathName;
            u32 nmbr = PopU32();
            if (!PopComma())
                return false;
            if (!PopToken(pathName))
                return false;
            if (nmbr >= (u32)m_files.size())
                m_files.resize(nmbr + 1);
            m_files[nmbr] = pathName;
        }
        return true;
    }

    bool PopSegment(bool validSegment)
    {
        std::string token;
        while (true)
        {
            PeekToken(token);
            if (token == "</")
                break;

            std::string blockTitle;
            std::string blockName;
            if (!PopHeaderStart(blockTitle, blockName))
                return false;
            if (blockTitle != "Block")
            {
                m_error = std::format("Expected Block, got {}", blockTitle);
                return false;
            }

            // START,END,FILE_IDX,LINE1,COL1,LINE2,COL2
            while (true)
            {
                std::string token;
                if (!PeekToken(token)) return false;
                if (token == "</")
                    break;

                DebugLine item;
                item.addrStart = PopAddr();
                if (!PopComma()) return false;
                item.addrEnd = PopAddr();
                if (!PopComma()) return false;
                item.fileNo = PopU32();
                if (!PopComma()) return false;
                item.line1 = PopU32();
                if (!PopComma()) return false;
                item.col1 = PopU32();
                if (!PopComma()) return false;
                item.line2 = PopU32();
                if (!PopComma()) return false;
                item.col2 = PopU32();

                if (validSegment)
                    m_debugLines.push_back(item);
            }
            if (!PopHeaderEnd(blockTitle))
                return false;
        }
        return true;
    }

    bool PopLabels()
    {
        while (true)
        {
            std::string token;
            if (!PeekToken(token))
                return false;
            if (token == "</")
                break;

            DebugLabel label;
            PopToken(token);
            if (!PopComma()) return false;
            label.addr = PopAddr();
            if (!PopComma()) return false;
            PopToken(label.name);
            if (!PopComma()) return false;
            label.fileNo = PopU32();
            if (!PopComma()) return false;
            label.line1 = PopU32();
            if (!PopComma()) return false;
            label.col1 = PopU32();
            if (!PopComma()) return false;
            label.line2 = PopU32();
            if (!PopComma()) return false;
            label.col2 = PopU32();
            m_debugLabels.push_back(label);
        }
        return true;
    }

    bool PopBreakpoints()
    {
        while (true)
        {
            std::string token;
            PeekToken(token);
            if (token == "</")
                break;
            PopToken(token);
        }
        return true;
    }

    bool PopWatchpoints()
    {
        while (true)
        {
            std::string token;
            PeekToken(token);
            if (token == "</")
                break;
            PopToken(token);
        }
        return true;
    }

    bool Parse(const std::string &segment)
    {
        std::string title;
        std::string titleName;
        if (!PopHeaderStart(title, titleName))
            return false;
        if (title != "C64debugger")
        {
            m_error = std::format("Expected C64Debugger, got {}", title);
            return false;
        }

        while (true)
        {
            std::string token;
            if (!PeekToken(token))
            {
                m_error = std::format("No end for section {}", title);
                return false;
            }
            if (token == "</")
            {
                break;
            }
            std::string section;
            std::string segmentName;
            PopHeaderStart(section, segmentName);
            if (section == "Sources")
            {
                if (!PopSources())
                    return false;
            }
            else if (section == "Segment")
            {
                bool validSegment = (segmentName == "Default") || StrEqualNoCase(segmentName, segment);
                if (!PopSegment(validSegment))
                    return false;
            }
            else if (section == "Labels")
            {
                if (!PopLabels())
                    return false;
            }
            else if (section == "Breakpoints")
            {
                if (!PopBreakpoints())
                    return false;
            }
            else if (section == "Watchpoints")
            {
                if (!PopWatchpoints())
                    return false;
            }
            else
            {
                m_error = std::format("Unknown section {}", section);
                return false;
            }
            PopHeaderEnd(section);
        }

        if (!PopHeaderEnd(title))
            return false;

        return true;
    }

    std::string m_error;
    std::vector<std::string> m_files;
    std::vector<DebugLine> m_debugLines;
    std::vector<DebugLabel> m_debugLabels;
};


DisassemblyInfo* LoadDisassemblyInternal(const std::filesystem::path& dbgFile)
{
    // load prg file
    auto prgFile = dbgFile;
    prgFile.replace_extension(".prg");

    u8* prgMem, * dbgMem;
    u32 prgSize, dbgSize;

    if (!LoadFile(prgFile, prgMem, prgSize))
    {
        return nullptr;
    }

    if (!LoadFile(dbgFile, dbgMem, dbgSize))
    {
        delete prgMem;
        return nullptr;
    }

    C64DebugParser parser((char*)dbgMem, dbgSize);
    parser.Parse(prgFile.stem().string());

    auto di = new DisassemblyInfo;
    di->m_dbgPath = dbgFile;
    di->m_memory = new u8[prgSize - 2];
    memcpy(di->m_memory, prgMem + 2, prgSize - 2);
    di->m_memoryAddress = (u32)prgMem[0] + ((u32)prgMem[1] << 8);
    di->m_memoryLength = prgSize - 2;

    for (size_t i = 0; i < parser.m_files.size(); i++)
    {
        auto file = new DisassemblyFile;
        file->m_info = di;
        file->m_path = parser.m_files[i];

        // work out the max lines we have for this file
        int maxLine = 0;
        for (auto& line : parser.m_debugLines)
        {
            if ((line.fileNo == i) && (line.addrStart <= (int)di->m_memoryAddress+(int)di->m_memoryLength) && (line.addrEnd > (int)di->m_memoryAddress))
                maxLine = Max(maxLine, line.line1);
        }

        if (maxLine > 0)
        {
            di->m_files.push_back(file);
            file->m_lines.resize(maxLine + 1);
            for (auto& line : parser.m_debugLines)
            {
                if (line.fileNo == i && !file->m_lines.empty())
                {
                    auto& dl = file->m_lines[line.line1];
                    if (dl.m_addressLength == 0)
                    {
                        dl.m_addressStart = line.addrStart;
                        dl.m_addressLength = line.addrEnd - line.addrStart + 1;
                    }
                    else
                    {
                        dl.m_addressLength = line.addrEnd - dl.m_addressStart + 1;
                    }
                }
            }
        }
    }

#if 0
    for (auto& str : parser.m_files)
    {
        Log(LogGroup::Build, "Files: {}", str);
    }
    for (auto& line : parser.m_debugLines)
    {
        Log(LogGroup::Build, "Lines: {}, ${:04x}..${:04x}, {}:{}, {}:{}", line.fileNo, line.addrStart, line.addrEnd, line.line1, line.col1, line.line2, line.col2);
    }
    for (auto& label : parser.m_debugLabels)
    {
        Log(LogGroup::Build, "Labels: {}, ${:04x}, {}:{}, {}:{}", label.name, label.fileNo, label.addr, label.line1, label.col1, label.line2, label.col2);
    }
#endif

    return di;
}


void SourceFileManager::InitKeywords(const char** keywords, SourceType sourceType, bool caseSensitive)
{
    const char** k = keywords;
    while (*k)
    {
        if (caseSensitive)
            m_keywords[(int)sourceType].insert(HashU8(0, *k));
        else
            m_keywords[(int)sourceType].insert(HashU8NoCase(0, *k));
        k++;
    }
}

SourceType ExtToSourceType(const std::string& ext)
{
    if (ext == ".c" || ext == ".C")
        return SourceType::C;
    if (ext == ".asm" || ext == ".ASM")
        return SourceType::Asm;
    if (ext == ".s" || ext == ".S")
        return SourceType::S;
    return SourceType::Text;
}

SourceFileManager::SourceFileManager()
{
    InitKeywords(s_keywords_asm, SourceType::Asm, false);
    InitKeywords(s_keywords_c, SourceType::C, true);
    InitKeywords(s_keywords_s, SourceType::S, true);
}

bool SourceFileManager::RenameFile(SourceFile* file, const std::string& path)
{
    file->m_path = path;
    std::filesystem::path fspath = path;
    std::string name = fspath.filename().string();
    file->m_sourceType = ExtToSourceType(fspath.extension().string());

    WindowMessageStruct msg;
    msg.m_type = WindowMessage::File_Renamed;
    msg.m_sourceFile = file;
    msg.m_flags = WMF_Window;
    WindowManager::Instance().Message(msg);
    WindowManager::Instance().LayoutWindows();
    return true;
}

bool SourceFileManager::NewFile(const std::string& path)
{
    auto sourceFile = new SourceFile(path);
    auto line = new SourceLine;
    sourceFile->m_lines.push_back(line);
    sourceFile->m_sourceType = SourceType::Asm;
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

void SourceFileManager::SetActiveSourceFile(class SourceFile* file)
{
    if (m_activeSourceFile != file)
    {
        m_activeSourceFile = file;

        WindowTree* tree;
        WindowLayout* layout;
        WindowBase* base;
        if (WindowManager::Instance().FindWindowByFile(file, tree, layout, base))
        {
            layout->ActivateWindow(base);
            tree->m_dirty = true;
        }
    }
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
                    msg.m_type = WindowMessage::Query_FileCount;
                    msg.m_flags = WMF_Window | WMF_EarlyOut;
                    msg.m_sourceFile = file;
                    WindowManager::Instance().Message(msg);
                    if (msg.m_response == 0)
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
        
        // load any debug files needed
        std::vector<std::filesystem::path> dbgFiles;
        for (auto& path : paths)
        {
            FindC64DebugFiles(path, dbgFiles);
        }

        // load any debug files we don't already have
        m_lockDisassembly.lock();
        for (auto& dbg : dbgFiles)
        {
            // lets load it
            auto loadDebug = [dbg]()
                {
                    SourceFileManager::Instance().LoadDisassembly(dbg, true);
                };
            std::thread(loadDebug).detach();
        }
        m_lockDisassembly.unlock();

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
            sl->m_assembledLine = (int)sourceFile->m_lines.size();
        }
        m_sourceFiles.push_back(sourceFile);
        m_activeSourceFile = sourceFile;

        WindowMessageStruct msg;
        msg.m_type = WindowMessage::File_Added;
        msg.m_flags = WMF_Window;
        msg.m_sourceFile = sourceFile;
        WindowManager::Instance().Message(msg);

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

void SourceFileManager::LoadDisassembly(const std::filesystem::path dbgPath, bool checkNotAlreadyLoaded)
{
    // check this dbgPath is not already loading...
    m_lockDisassembly.lock();
    if (std::find(m_loadingDisassemblies.begin(), m_loadingDisassemblies.end(), dbgPath) != m_loadingDisassemblies.end())
    {
        m_lockDisassembly.unlock();
        return;
    }

    if (checkNotAlreadyLoaded)
    {
        // are we already queued to load it?
        auto cmpDis = [dbgPath](const DisassemblyInfo* d) { return d->m_dbgPath == dbgPath; };
        if (std::find_if(m_queuedForAddDisassemblies.begin(), m_queuedForAddDisassemblies.end(), cmpDis) != m_queuedForAddDisassemblies.end())
        {
            m_lockDisassembly.unlock();
            return;
        }

        // have we already loaded it?
        if (std::find_if(m_activeDisassemblies.begin(), m_activeDisassemblies.end(), cmpDis) != m_activeDisassemblies.end())
        {
            m_lockDisassembly.unlock();
            return;
        }
    }

    // add it to the list of loading dbgs and we can release the lock now and do the actual loading
    m_loadingDisassemblies.push_back(dbgPath);
    m_lockDisassembly.unlock();

    auto disassembly = LoadDisassemblyInternal(dbgPath);
    if (disassembly)
    {
        SourceFileManager::Instance().AddDisassembly(disassembly);
        WindowMessageStruct msg;
        msg.m_type = WindowMessage::File_Compiled;
        msg.m_flags = WMF_Window;
        WindowManager::Instance().QueueDeferredMessage(msg);
    }

    // all done, remove it from the loading list
    m_lockDisassembly.lock();
    auto it = std::find(m_loadingDisassemblies.begin(), m_loadingDisassemblies.end(), dbgPath);
    if (it != m_loadingDisassemblies.end())
    {
        *it = std::move(m_loadingDisassemblies.back());
        m_loadingDisassemblies.pop_back();
    }
    m_lockDisassembly.unlock();
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
        file->m_cmdBuffer->Mark();

    return fh.good();
}

bool SourceFileManager::CloseFile(SourceFile* file)
{
    if (m_activeSourceFile == file)
        m_activeSourceFile = nullptr;

    m_sourceFiles.erase(std::find(m_sourceFiles.begin(), m_sourceFiles.end(), file));

    WindowMessageStruct msg;
    msg.m_type = WindowMessage::File_Deleted;
    msg.m_flags = WMF_Window;
    msg.m_sourceFile = file;
    WindowManager::Instance().Message(msg);
    WindowManager::Instance().RemoveQueuedWindows();

    // delete the file
    delete file;

    WindowManager::Instance().LayoutWindows();
    return true;
}

void SourceFileManager::Run(const std::filesystem::path& outputFile)
{
    if (outputFile.extension() == ".d64")
    {
        std::string cmd = std::format("start F:\\Emulators\\C64\\Vice3.6\\bin\\x64sc.exe {}", outputFile.string());
        Application::Instance().SendShellCommand(cmd);
    }
    else if (outputFile.extension() == ".prg")
    {
        std::string cmd = std::format("start F:\\Emulators\\C64\\Vice3.6\\bin\\x64sc.exe {}", outputFile.string());
        Application::Instance().SendShellCommand(cmd);
    }
}

void SourceFileManager::Deploy(const std::filesystem::path& outputFile)
{
    u8* mem;
    size_t size;
    if (LoadBinaryFile(outputFile, mem, size))
    {
        if (outputFile.extension() == ".prg")
        {
            auto msg = new NMS_Command("runners:run_prg", mem, size, outputFile.filename().string(), false);
            NetworkManager::Instance().Message(msg);
        }
        else
        {
            auto msg = new NMS_Command("drives/{}:mount?type=d64&mode=unlinked", mem, size, outputFile.filename().string(), false);
            NetworkManager::Instance().Message(msg);
        }
    }
}

void SourceFileManager::Run(class SourceFile* file)
{
    std::filesystem::path path = file->m_path;
    const std::filesystem::path outputFolder = path.parent_path() / "out";
    const std::filesystem::path filename = path.stem();

    const std::string d64Path = (outputFolder / filename).replace_extension(".d64").string();
    const std::string prgPath = (outputFolder / filename).replace_extension(".prg").string();

    auto paths = FindC64Outputs(outputFolder);
    if (!paths.empty())
    {
        Run(paths[0]);
    }
}

void SourceFileManager::Compile(SourceFile* file)
{
    SaveAll();

    std::filesystem::path path = file->m_path;
    if (file->m_sourceType == SourceType::Asm)
    {
        // mark all lines
        int ln = 1;
        for (auto line : file->m_lines)
            line->m_assembledLine = ln++;

        auto compileKickAss = [file]()
            {
                LogManager::Instance().Clear(LogGroup::Build);

                std::filesystem::path path = file->m_path;
                std::filesystem::path workingDir = path.parent_path() / "out";
                std::error_code ec;
                if (!std::filesystem::create_directories(workingDir, ec) && ec)
                {
                    Log(LogGroup::Build,
                        "Failed to create directory '{}': {}\n",
                        workingDir.string(),
                        ec.message());
                    return;
                }

                std::string outSymbol = path.filename().replace_extension(".sym").string();
                std::filesystem::path filePath = file->m_path;
                std::string expectLine = std::format("Writing Symbol file: {}", outSymbol);
                auto compileWatcher = [expectLine, filePath](const std::string &line)->bool
                    {
                        if (line == expectLine)
                        {
                            auto assemble = [filePath]()
                                {
                                    auto dbgFile = (filePath.parent_path() / "out" / filePath.stem()).replace_extension(".dbg");
                                    SourceFileManager::Instance().LoadDisassembly(dbgFile, false);
                                };
                            std::thread(assemble).detach();
                            return true;
                        }
                        return false;
                    };
                Application::Instance().AddShellWatcherOnce(compileWatcher);

                std::string cmd = std::format("java -jar kickass\\kickass.jar {} -bytedump -debugdump -odir {}", file->m_path, workingDir.string());
                Application::Instance().SendShellCommand(cmd);
            };
        std::thread(compileKickAss).detach();
    }
    else if (file->m_sourceType == SourceType::C)
    {
        auto compileCC65 = [file]()
            {
                LogManager::Instance().Clear(LogGroup::Build);

                std::filesystem::path path = file->m_path;
                std::filesystem::path filename = path.filename();
                std::filesystem::path workingDir = path.parent_path() / "out";
                std::error_code ec;
                if (!std::filesystem::create_directories(workingDir, ec) && ec)
                {
                    Log(LogGroup::Build, "Failed to create directory '{}': {}\n", workingDir.string(), ec.message());
                    return;
                }

                std::string path_c = path.string();;
                std::string path_s = (workingDir / filename.replace_extension(".s")).string();
                std::string path_o = (workingDir / filename.replace_extension(".o")).string();
                std::string path_prg = (workingDir / filename.replace_extension(".prg")).string();
                std::string path_dbg = (workingDir / filename.replace_extension(".dbg")).string();

                std::string cwd = std::filesystem::current_path().string();
                std::string cmdCompile = std::format("{}/cc65/bin/cc65.exe -O -t c64 {} -o {} -g", cwd, path_c, path_s);
                std::string cmdAssembly = std::format("{}/cc65/bin/ca65.exe -t c64 {} -o {} -g", cwd, path_s, path_o);
                std::string cmdLink = std::format("{}/cc65/bin/ld65.exe -t c64 {} c64.lib -o {} --dbgfile {}", cwd, path_o, path_prg, path_dbg);
                
                Application::Instance().SendShellCommand(cmdCompile);
                Application::Instance().SendShellCommand(cmdAssembly);
                Application::Instance().SendShellCommand(cmdLink);
            };
        std::thread(compileCC65).detach();
    }
    else
    {
        Log(LogGroup::Build, "Unsupported extension");
    }
}

void SourceFileManager::RestoreFilesFromSettings()
{
    auto strList = Settings::Instance().GetStringList(SETTING_FILES);
    RequestLoadFiles(strList);
    LoadRequestedFiles(false);
}

void SourceFileManager::SaveFilesToSettings()
{
    std::vector<std::string> filelist;
    for (auto file : m_sourceFiles)
        filelist.push_back(file->m_path);
    Settings::Instance().SetStringList(SETTING_FILES, filelist);
}

void SourceFileManager::AddDisassembly(DisassemblyInfo* info)
{
    m_lockDisassembly.lock();
    m_queuedForAddDisassemblies.push_back(info);
    m_lockDisassembly.unlock();
}

void SourceFileManager::UpdateDisassemblies()
{
    m_lockDisassembly.lock();
    for (auto addDis : m_queuedForAddDisassemblies)
    {
        // replace existing disassembly if the path matches
        bool replaced = false;
        for (int i = 0; i < m_activeDisassemblies.size(); i++)
        {
            auto activeDis = m_activeDisassemblies[i];
            if (StrEqualNoCase(activeDis->m_dbgPath.string(), addDis->m_dbgPath.string()))
            {
                delete activeDis;
                m_activeDisassemblies[i] = addDis;
                replaced = true;
                break;
            }
        }
        if (!replaced)
            m_activeDisassemblies.push_back(addDis);
    }
    m_queuedForAddDisassemblies.clear();
    m_lockDisassembly.unlock();
}

DisassemblyFile *SourceFileManager::GetDisassembly(SourceFile* file)
{
    for (auto active : m_activeDisassemblies)
    {
        for (auto di : active->m_files)
        {
            if (di->m_path == file->m_path)
                return di;
        }
    }
    return nullptr;
}

