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

const char* s_keywords_asm[] = { "tax", "eor", "dec", "pla", "rts", "rti", "bcc", "txa", "clc", "sec", "cpx", "cpy", "cmp", "bne", "beq", "bmi", "bpl", "ldx", "ldy", "stx", "sty", "jsr", "jmp", "tay", "tya", "pha", "dey", "dex", "inc", "inx", "iny", "lda", "sta", "adc", "lsr", "asr", "asl", "lsl", "and", "ora", "xor", "sei", "cli", 0};
const char* s_keywords_c[] = { "char", "int", "long", "short", "(", ")", ";", "{", "}", "[", "]",
            "void", "while", "for", "if", "else", "#define", "#ifdef", "#include", "//",
            "<", ">", "=", "==", "!=", "+", "-", "*", "/", "++", "--", "+=", "-=", 0};

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
    msg.m_flags = WMF_Window;
    WindowManager::Instance().Message(msg);
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


bool LoadBinaryFile(const std::filesystem::path& outputFile, u8 * &mem, size_t& outputSize)
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
        auto compileKickAss = [file]()
            {
                LogManager::Instance().Clear(LogGroup::Build);

                std::filesystem::path workingDir = std::filesystem::path(file->m_path).parent_path() / "out";
                std::error_code ec;
                if (!std::filesystem::create_directories(workingDir, ec) && ec)
                {
                    Log(LogGroup::Build,
                        "Failed to create directory '{}': {}\n",
                        workingDir.string(),
                        ec.message());
                    return;
                }

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
