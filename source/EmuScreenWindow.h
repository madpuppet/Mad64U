#pragma once

#include "WindowBase.h"

class EmuScreenWindow : public WindowBase
{
public:
    EmuScreenWindow();
    ~EmuScreenWindow();
    void Paint(SDL_Renderer* renderer, const Recti& dirtyArea) override;
    bool HandleEvent(SDL_Event* e) override;

    void SaveTokens(std::vector<std::string>& layoutTokens) override;
    static bool CreateFromLayoutTokens(struct WindowLayout* layout, const std::vector<std::string>& layoutTokens, size_t& idx);

    void UpdateTexture(int w, int h);

protected:
    SDL_Texture* viceTexture = nullptr;
    int viceTextureWidth = 0;
    int viceTextureHeight = 0;
};
