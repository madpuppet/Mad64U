#pragma once

#include "WindowBase.h"
#include <filesystem>

class DisAssemblyWindow : public WindowBase
{
public:
    DisAssemblyWindow();
    ~DisAssemblyWindow();

    void Paint(SDL_Renderer* renderer, const Recti& dirtyArea) override;
    bool HandleEvent(SDL_Event* e) override;
    bool Tick() override;
    void SaveTokens(std::vector<std::string>& layoutTokens) override;
    static bool CreateFromLayoutTokens(WindowLayout* layout, const std::vector<std::string>& layoutTokens, size_t& idx);
    void MessageChild(WindowLayout* layout, struct WindowMessageStruct& msg) override;

protected:
    struct Line
    {
        u16 addr;
        int bytes;
        int cycles;
        std::string opcodeStr;
        std::string operandStr;
    };
    std::vector<Line> m_lines;
    u16 m_cacheAddr = 0;
    void BuildLines();
    int DisassembleLine(class Cpu6502 &cpu, u8* ram, int l, u16 addr);
};

#pragma once
