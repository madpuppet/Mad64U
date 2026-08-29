#include "common.h"
#include "Application.h"
#include <SDL3/SDL_main.h>
#include <fstream>

int main(int argc, char* argv[])
{
    Application::Startup();
    return Application::Instance().Run();
}

void Log(const char* pFormat, ...)
{
    va_list va;
    va_start(va, pFormat);
    char buffer[1024];
    vsprintf(buffer, pFormat, va);

    FILE* fh = fopen("log.txt", "a");
    if (fh)
    {
        fprintf(fh, "%s", buffer);
        fclose(fh);
    }

#if defined(_WIN64)
    OutputDebugStringA(buffer);
#endif
}

bool LoadFile(const std::filesystem::path& path, u8*& mem, u32& size)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file)
        return {};

    const std::streamsize fsize = file.tellg();
    if (fsize < 0)
        return {};

    size = (u32)fsize;
    file.seekg(0, std::ios::beg);

    if (size == 0)
    {
        mem = nullptr;
        size = 0;
        return false;
    }

    mem = new u8[size];
    if (!file.read((char*)mem, size))
    {
        delete[] mem;
        mem = nullptr;
        return false;
    }

    return true;
}

