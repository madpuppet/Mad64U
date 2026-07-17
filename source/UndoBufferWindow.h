#pragma once

#include "WindowBase.h"

class UndoBufferWindow : public WindowBase
{
public:
    UndoBufferWindow();
    ~UndoBufferWindow();

    void Paint(SDL_Renderer* renderer, const Recti& dirtyArea) override;
    bool HandleEvent(SDL_Event* e) override;
    bool Tick() override;
    void SaveTokens(std::vector<std::string>& layoutTokens) override;
    static bool CreateFromLayoutTokens(WindowLayout* layout, const std::vector<std::string>& layoutTokens, size_t& idx);

protected:
};

