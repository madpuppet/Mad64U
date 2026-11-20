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
    int m_zoom = 0;
    float m_animTime = 0.0f;
    bool m_warpMode = false;
    bool m_shift = false;

    // the C64 key each held PC key pressed, so the release goes to the same key
    struct HeldKey
    {
        bool m_held = false;
        int m_row = 0;
        int m_column = 0;
        int m_r2 = -1;
        int m_c2 = -1;
        bool m_cancelShift = false;
    };
    HeldKey m_heldKeys[SDL_SCANCODE_COUNT];
};
