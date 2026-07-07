#include "common.h"
#include "Application.h"
#include <SDL3/SDL_main.h>

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
