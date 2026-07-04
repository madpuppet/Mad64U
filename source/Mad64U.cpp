#include "common.h"
#include "Application.h"
#include <SDL3/SDL_main.h>

int main(int argc, char* argv[])
{
    auto app = new Application;
    int rc = app->Run(argc);
    return rc;
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
