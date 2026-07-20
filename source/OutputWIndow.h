#pragma once

#include "WindowBase.h"
#include "LogManager.h"

class OutputWindow : public WindowBase
{
public:
    OutputWindow(LogGroup group);
    void MessageChild(struct WindowLayout* layout, struct WindowMessageStruct& msg) override;
    void Paint(SDL_Renderer* renderer, const Recti& dirtyArea) override;
    bool HandleEvent(SDL_Event* e) override;
    void SaveTokens(std::vector<std::string>& layoutTokens) override;
    static bool CreateFromLayoutTokens(struct WindowLayout* layout, const std::vector<std::string>& layoutTokens, size_t& idx);

protected:
    LogGroup m_activeGroup;
    size_t m_lastSize = 0;
};