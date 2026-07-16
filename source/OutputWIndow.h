#pragma once

#include "WindowBase.h"
#include "LogManager.h"

class OutputWindow : public WindowBase
{
public:
    OutputWindow();
    void Paint(SDL_Renderer* renderer, const Recti& dirtyArea) override;
    bool HandleEvent(SDL_Event* e) override;
    void SaveTokens(std::vector<std::string>& layoutTokens) override;
    static bool CreateFromLayoutTokens(WindowLayout* layout, const std::vector<std::string>& layoutTokens, size_t& idx);

protected:
    LogGroup m_activeGroup;
};