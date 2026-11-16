#pragma once

#include "WindowBase.h"
#include "ViceBridge.h"

// Shows the current state of the SID chip: per voice frequency / pulse width / waveform / control bits / ADSR and
// live envelope level, a waveform display for each voice, and the filter and volume settings.
//
// The waveform displays are oscilloscopes of each voice's real output (oscillator x envelope), captured from reSID as
// the chip is clocked. They are triggered on a rising zero crossing; the header zoom icon changes the timebase.
class SidWindow : public WindowBase
{
public:
    SidWindow();
    ~SidWindow();
    void Paint(SDL_Renderer* renderer, const Recti& dirtyArea) override;
    void MessageChild(WindowLayout* layout, struct WindowMessageStruct& msg) override;
    bool Tick() override;
    bool HandleEvent(SDL_Event* e) override;

    // true if a newer SID state than the one being displayed has been captured
    bool NeedsRefresh();

    void SaveTokens(std::vector<std::string>& layoutTokens) override;
    static bool CreateFromLayoutTokens(struct WindowLayout* layout, const std::vector<std::string>& layoutTokens, size_t& idx);

protected:
    void DrawVoice(SDL_Renderer* renderer, int voice, const SDL_FRect& area, int charWidth);
    void DrawWaveform(SDL_Renderer* renderer, int voice, const SDL_FRect& area);

    SidState m_state;
    u32 m_stateVersion = 0;
    std::vector<SDL_FPoint> m_points;
    int m_timebase = 2;         // index into the timebase table (5ms)
};
