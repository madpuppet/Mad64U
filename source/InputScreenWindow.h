#pragma once

#include "WindowBase.h"
#include "ViceBridge.h"

// Shows the current state of the C64 inputs as the emulated machine sees them:
//  - the keyboard, drawn as a C64 keyboard with held keys lit, plus the raw 8x8 CIA1 key matrix. Each key also shows
//    what it types with C= (bottom left) and SHIFT (bottom right), taken from the KERNAL's own decode tables and drawn
//    with the character ROM; colour codes show as colour swatches
//  - both control port joysticks (directions and fire)
//  - the paddle (pot) values of each control port, and which port the SID is reading them from
class InputScreenWindow : public WindowBase
{
public:
    InputScreenWindow();
    ~InputScreenWindow();
    void Paint(SDL_Renderer* renderer, const Recti& dirtyArea) override;
    void MessageChild(WindowLayout* layout, struct WindowMessageStruct& msg) override;
    bool Tick() override;
    bool HandleEvent(SDL_Event* e) override;

    // true if a newer input state than the one being displayed has been captured
    bool NeedsRefresh();

    void SaveTokens(std::vector<std::string>& layoutTokens) override;
    static bool CreateFromLayoutTokens(struct WindowLayout* layout, const std::vector<std::string>& layoutTokens, size_t& idx);

protected:
    void DrawKeyboard(SDL_Renderer* renderer, float x, float y, float unit);
    float DrawMatrix(SDL_Renderer* renderer, float x, float y);
    float DrawJoystick(SDL_Renderer* renderer, int port, float x, float y);
    float DrawPaddles(SDL_Renderer* renderer, float x, float y);

    void UpdateGlyphTexture(SDL_Renderer* renderer);
    void DrawKeyCode(SDL_Renderer* renderer, u8 petscii, float x, float y, float size, const SDL_Color& col);

    InputState m_state;
    u32 m_stateVersion = 0;

    // controller mode button above the keyboard: click for the next mode, right click for the previous one
    SDL_FRect m_modeButton{ 0, 0, 0, 0 };
    ControllerMode m_shownMode = ControllerMode::Count;     // mode last drawn, so a change from the menu repaints

    // what each key types, from the KERNAL decode tables: [normal, SHIFT, C=, CTRL][matrix position]
    u8 m_keyTables[4][64] = {};
    bool m_haveKeyTables = false;

    // the upper case / graphics character set (first 128 characters of the char ROM) as a 128x64 white-on-clear
    // texture, tinted when drawn
    u8 m_charRom[0x1000] = {};
    u8 m_glyphRom[0x400] = {};      // the char ROM data the texture was built from
    SDL_Renderer* m_glyphRenderer = nullptr;
    SDL_Texture* m_glyphTexture = nullptr;
};
