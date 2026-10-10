#pragma once

#include "WindowBase.h"
#include <filesystem>

enum class MemViewMode
{
    Bytes,
    Words,
    Sprite,
    Char,
    Screen,
    Bitmap
};

class MemViewWindow : public WindowBase
{
public:
    MemViewWindow();
    ~MemViewWindow();

    void Paint(SDL_Renderer* renderer, const Recti& dirtyArea) override;
    bool HandleEvent(SDL_Event* e) override;
    bool Tick() override;
    void SaveTokens(std::vector<std::string>& layoutTokens) override;
    static bool CreateFromLayoutTokens(WindowLayout* layout, const std::vector<std::string>& layoutTokens, size_t& idx);
    void MessageChild(WindowLayout* layout, struct WindowMessageStruct& msg) override;

protected:
    MemViewMode m_memViewMode = MemViewMode::Bytes;
    int m_bytesPerLine;
    int m_lineCount;
    void CalcModeInfo();
};

#pragma once
