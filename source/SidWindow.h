#pragma once

#include "WindowBase.h"
#include "ViceBridge.h"

// Shows the current state of the SID chip: per voice frequency / pulse width / waveform / control bits / ADSR and
// live envelope level, a waveform display for each voice, and the filter and volume settings.
//
// The waveform displays are drawn from the chip state (waveform select, pulse width, sync / ring modulation with the
// neighbouring voice, and the live envelope level) rather than from recorded audio, so they are stable and show the
// shape of what each oscillator is producing right now.
class SidWindow : public WindowBase
{
public:
    SidWindow();
    ~SidWindow();
    void Paint(SDL_Renderer* renderer, const Recti& dirtyArea) override;
    void MessageChild(WindowLayout* layout, struct WindowMessageStruct& msg) override;
    bool Tick() override;

    // true if a newer SID state than the one being displayed has been captured
    bool NeedsRefresh();

    void SaveTokens(std::vector<std::string>& layoutTokens) override;
    static bool CreateFromLayoutTokens(struct WindowLayout* layout, const std::vector<std::string>& layoutTokens, size_t& idx);

protected:
    void DrawVoice(SDL_Renderer* renderer, int voice, const SDL_FRect& area, int charWidth);
    void DrawWaveform(SDL_Renderer* renderer, int voice, const SDL_FRect& area);

    SidState m_state;
    u32 m_stateVersion = 0;
    std::vector<float> m_wave;
    std::vector<SDL_FPoint> m_points;
};
