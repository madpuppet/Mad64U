#pragma once

#include "WindowBase.h"

class FunctionsWindow : public WindowBase
{
public:
    FunctionsWindow();
    ~FunctionsWindow();

    void MessageChild(WindowLayout* layout, struct WindowMessageStruct& msg);
    void Paint(SDL_Renderer* renderer, const Recti& dirtyArea) override;
    bool HandleEvent(SDL_Event* e) override;
    bool Tick() override;
    void SaveTokens(std::vector<std::string>& layoutTokens) override;
    static bool CreateFromLayoutTokens(WindowLayout* layout, const std::vector<std::string>& layoutTokens, size_t& idx);

protected:
    void RebuildLines();
    struct FunctionLine
    {
        int m_line;
        std::string m_label;
    };

    std::vector<FunctionLine> m_functionLines;
    int m_cachedFileID = 0;
    int m_cachedLinesSize = 0;
};


