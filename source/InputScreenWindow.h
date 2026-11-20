#pragma once

#include "WindowBase.h"
#include "ViceBridge.h"

// Shows the current state of the C64 inputs as the emulated machine sees them:
//  - the keyboard, drawn as a C64 keyboard with held keys lit, plus the raw 8x8 CIA1 key matrix
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

    // true if a newer input state than the one being displayed has been captured
    bool NeedsRefresh();

    void SaveTokens(std::vector<std::string>& layoutTokens) override;
    static bool CreateFromLayoutTokens(struct WindowLayout* layout, const std::vector<std::string>& layoutTokens, size_t& idx);

protected:
    void DrawKeyboard(SDL_Renderer* renderer, float x, float y, float unit);
    float DrawMatrix(SDL_Renderer* renderer, float x, float y);
    float DrawJoystick(SDL_Renderer* renderer, int port, float x, float y);
    float DrawPaddles(SDL_Renderer* renderer, float x, float y);

    InputState m_state;
    u32 m_stateVersion = 0;
};
