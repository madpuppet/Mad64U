#pragma once

#include "WindowBase.h"

class EmuScreenWindow : public WindowBase
{
public:
    EmuScreenWindow();
    ~EmuScreenWindow();
    void Paint(SDL_Renderer* renderer, const Recti& dirtyArea) override;
    bool HandleEvent(SDL_Event* e) override;
    void MessageChild(WindowLayout* layout, struct WindowMessageStruct& msg) override;
    bool Tick() override;

    void SaveTokens(std::vector<std::string>& layoutTokens) override;
    static bool CreateFromLayoutTokens(struct WindowLayout* layout, const std::vector<std::string>& layoutTokens, size_t& idx);

    void UpdateTexture(SDL_Renderer* renderer, struct ViceFrame &frame);

protected:
    SDL_Renderer* m_renderer = nullptr;
    SDL_Texture* m_viceTexture = nullptr;
    int m_viceTextureWidth = 0;
    int m_viceTextureHeight = 0;
    bool m_snapToClientArea = false;
    int m_zoom = 3;
    float m_animTime = 0.0f;
};
