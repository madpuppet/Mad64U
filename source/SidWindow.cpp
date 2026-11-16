#include "common.h"
#include "SidWindow.h"
#include "WindowLayout.h"
#include "ViceBridge.h"
#include "LogManager.h"
#include "WindowManager.h"
#include "Application.h"

#define SID_ROW_HEIGHT 18
#define SID_INFO_CHARS 40
#define SID_VOICE_LINES 6

#define SID_CLOCK_PAL 985248.0
#define SID_CLOCK_NTSC 1022727.0

// attack times in ms for each rate; decay and release take 3 times as long
static const int s_attackMs[16] = { 2, 8, 16, 24, 38, 56, 68, 80, 100, 250, 500, 800, 1000, 3000, 5000, 8000 };

static const char* s_noteNames[12] = { "C-", "C#", "D-", "D#", "E-", "F-", "F#", "G-", "G#", "A-", "A#", "B-" };
static const char* s_envStateNames[4] = { "ATTACK", "DECAY/SUSTAIN", "RELEASE", "FROZEN" };

static const SDL_Color s_voiceColours[3] = { { 110, 220, 110, 255 }, { 110, 200, 255, 255 }, { 255, 200, 90, 255 } };
static const SDL_Color s_labelCol{ 160, 160, 160, 255 };
static const SDL_Color s_valueCol{ 255, 255, 255, 255 };
static const SDL_Color s_offCol{ 80, 80, 80, 255 };

// oscilloscope timebases: time shown across each voice scope
static const float s_timebaseMs[] = { 1.0f, 2.0f, 5.0f, 10.0f, 20.0f, 50.0f };
static const int s_timebaseCount = (int)(sizeof(s_timebaseMs) / sizeof(s_timebaseMs[0]));

static std::string FormatMs(int ms)
{
    if (ms >= 1000)
        return std::format("{:.1f}s", ms / 1000.0f);
    return std::format("{}ms", ms);
}

SidWindow::SidWindow()
{
    m_name = "Sid";
}

SidWindow::~SidWindow()
{
}

void SidWindow::MessageChild(WindowLayout* layout, struct WindowMessageStruct& msg)
{
    switch (msg.m_type)
    {
        case WindowMessage::Query_Highlight:
        {
            auto query = (WindowHighlightQuery*)msg.m_query;

            // timebase (zoom) icon in the header
            Recti iconArea = IconRenderer::Instance().CalcIconArea(Icons::Zoom, m_clientArea.x + 16, m_clientArea.y + 12);
            if (iconArea.Contains(msg.m_x, msg.m_y))
            {
                query->m_area = iconArea;
                query->m_highlight = WindowHighlightType::EmuScreenIcon;
                query->m_id = 0;
                query->m_emuScreen.m_icon = Icons::Zoom;
                query->m_tree = msg.m_tree;
                query->m_layout = layout;
                query->m_window = this;
                msg.m_response++;
                return;
            }

            if (m_clientArea.Contains(msg.m_x, msg.m_y))
            {
                msg.m_response++;
                query->m_area = m_clientArea;
                query->m_highlight = WindowHighlightType::ClientArea;
                query->m_tree = msg.m_tree;
                query->m_layout = layout;
                query->m_window = this;
                return;
            }
        }
        break;
    }
}

bool SidWindow::HandleEvent(SDL_Event* e)
{
    if (e->type == SDL_EVENT_MOUSE_BUTTON_DOWN && e->button.button == SDL_BUTTON_LEFT)
    {
        auto& selected = WindowManager::Instance().GetWindowSelectionQuery();
        if (selected.m_highlight == WindowHighlightType::EmuScreenIcon && selected.m_window == this && selected.m_emuScreen.m_icon == Icons::Zoom)
        {
            // shift-click zooms in, click zooms out
            if (SDL_GetModState() & SDL_KMOD_SHIFT)
                m_timebase = (m_timebase + s_timebaseCount - 1) % s_timebaseCount;
            else
                m_timebase = (m_timebase + 1) % s_timebaseCount;
            return true;
        }
    }
    return false;
}

bool SidWindow::Tick()
{
    return NeedsRefresh();
}

bool SidWindow::NeedsRefresh()
{
    return ViceBridge::Exists() && ViceBridge::Instance().GetSidStateVersion() != m_stateVersion;
}

//==================================================================================================================================
// Oscilloscope
//
// Plots each voice's real output, captured from reSID as the chip is clocked (one sample every SID_SCOPE_DECIMATE
// cycles, ~246kHz). Like a real scope the trace is triggered on a rising zero crossing so periodic waveforms stand still,
// and each pixel column draws the min..max of the samples it covers so no detail is lost when zoomed out.
//==================================================================================================================================

// trigger hysteresis: the signal must come from below -this before crossing zero
#define SCOPE_TRIGGER_HYSTERESIS 512

void SidWindow::DrawWaveform(SDL_Renderer* renderer, int voice, const SDL_FRect& area)
{
    if (area.w < 16.0f || area.h < 16.0f)
        return;

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderFillRect(renderer, &area);

    // grid: 10 time divisions, centre line and +-half scale
    float midY = area.y + area.h * 0.5f;
    SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
    for (int d = 1; d < 10; d++)
    {
        float x = area.x + area.w * d / 10.0f;
        SDL_RenderLine(renderer, x, area.y, x, area.y + area.h);
    }
    SDL_RenderLine(renderer, area.x, area.y + area.h * 0.25f, area.x + area.w, area.y + area.h * 0.25f);
    SDL_RenderLine(renderer, area.x, area.y + area.h * 0.75f, area.x + area.w, area.y + area.h * 0.75f);
    SDL_SetRenderDrawColor(renderer, 60, 60, 60, 255);
    SDL_RenderLine(renderer, area.x, midY, area.x + area.w, midY);
    SDL_RenderRect(renderer, &area);

    auto& scope = ViceBridge::Instance().GetSidScope();
    const i16* samples = scope.m_samples[voice];
    const u32 mask = SID_SCOPE_SAMPLES - 1;
    u32 writeIdx = scope.m_writeIdx.load(std::memory_order_acquire);

    double clock = Application::Instance().IsPAL() ? SID_CLOCK_PAL : SID_CLOCK_NTSC;
    double sampleRate = clock / SID_SCOPE_DECIMATE;
    u32 span = (u32)(sampleRate * s_timebaseMs[m_timebase] / 1000.0);
    span = std::clamp(span, 16u, (u32)(SID_SCOPE_SAMPLES / 3));
    if (writeIdx < span + 2)
        return;

    // find the most recent rising zero crossing that still leaves a full span after it (trigger at 10% from the left).
    // search back up to one more span; if there is none (silence, noise-free DC) the trace free runs.
    u32 pre = span / 10;
    u32 newest = writeIdx - 1;
    u32 latestTrigger = newest - (span - pre);
    u32 searchLimit = (std::min)(writeIdx - 1, span * 2);
    u32 start = writeIdx - span;
    bool armed = false;
    // oldest sample still in the ring, leaving room for the hysteresis look-back
    u32 oldest = (writeIdx > SID_SCOPE_SAMPLES ? writeIdx - SID_SCOPE_SAMPLES : 0) + 64;
    for (u32 back = 0; back < searchLimit && latestTrigger >= oldest + back; back++)
    {
        // walk backwards: look for "at or above zero" preceded (earlier in time) by "below -hysteresis"
        u32 t = latestTrigger - back;
        i16 cur = samples[t & mask];
        i16 prev = samples[(t - 1) & mask];
        if (cur >= 0 && prev < 0)
        {
            // confirm the signal was properly below the hysteresis level shortly before
            armed = false;
            for (u32 k = 1; k < 64 && k <= t; k++)
            {
                if (samples[(t - k) & mask] < -SCOPE_TRIGGER_HYSTERESIS)
                {
                    armed = true;
                    break;
                }
                if (samples[(t - k) & mask] > 0)
                    break;
            }
            if (armed)
            {
                start = t - pre;
                break;
            }
        }
    }

    // one point pair (min, max) per pixel column
    int columns = (std::max)(1, (int)area.w - 2);
    m_points.resize(columns * 2);
    float amp = (area.h * 0.5f - 2.0f) / 32768.0f;
    int n = 0;
    for (int c = 0; c < columns; c++)
    {
        u32 s0 = start + (u32)((u64)span * c / columns);
        u32 s1 = start + (u32)((u64)span * (c + 1) / columns);
        if (s1 <= s0)
            s1 = s0 + 1;

        int lo = 32767, hi = -32768;
        for (u32 s = s0; s < s1; s++)
        {
            int v = samples[s & mask];
            lo = (std::min)(lo, v);
            hi = (std::max)(hi, v);
        }

        float x = area.x + 1.0f + c;
        // alternate the order so the polyline zig-zags cleanly between neighbouring columns
        if (c & 1)
        {
            m_points[n++] = { x, midY - hi * amp };
            m_points[n++] = { x, midY - lo * amp };
        }
        else
        {
            m_points[n++] = { x, midY - lo * amp };
            m_points[n++] = { x, midY - hi * amp };
        }
    }

    SDL_Color col = s_voiceColours[voice];
    SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, 255);
    SDL_RenderLines(renderer, m_points.data(), n);

    // trigger marker
    if (armed)
    {
        float tx = area.x + 1.0f + columns * (float)pre / span;
        SDL_SetRenderDrawColor(renderer, col.r / 2, col.g / 2, col.b / 2, 255);
        SDL_RenderLine(renderer, tx, area.y + 1, tx, area.y + 6);
    }
}
void SidWindow::DrawVoice(SDL_Renderer* renderer, int voice, const SDL_FRect& area, int charWidth)
{
    auto& fr = FontRenderer::Instance();
    const u8* r = &m_state.m_regs[voice * 7];
    u8 filterRouting = m_state.m_regs[0x17];
    bool voice3Off = voice == 2 && (m_state.m_regs[0x18] & 0x80);

    u32 freq = r[0] | (r[1] << 8);
    u32 pw = (r[2] | (r[3] << 8)) & 0xfff;
    u8 ctrl = r[4];
    int attack = r[5] >> 4, decay = r[5] & 15, sustain = r[6] >> 4, release = r[6] & 15;

    double clock = Application::Instance().IsPAL() ? SID_CLOCK_PAL : SID_CLOCK_NTSC;
    double hz = freq * clock / 16777216.0;

    // draws a string and returns the x position after it (plus a space)
    auto text = [&](int x, int y, const std::string& str, const SDL_Color& col) -> int
        {
            fr.RenderText(renderer, str, col, x, y, FontType::Text);
            return x + ((int)str.size() + 1) * charWidth;
        };
    auto flag = [&](int x, int y, const char* name, bool on) -> int
        {
            return text(x, y, name, on ? s_valueCol : s_offCol);
        };

    int x0 = (int)area.x + 8;
    int labelW = 7 * charWidth;
    int y = (int)area.y + 4;

    // line 1: voice + control bits
    int x = text(x0, y, std::format("VOICE {}", voice + 1), s_voiceColours[voice]);
    x += charWidth;
    x = flag(x, y, "GATE", ctrl & 0x01);
    x = flag(x, y, "SYNC", ctrl & 0x02);
    x = flag(x, y, "RING", ctrl & 0x04);
    x = flag(x, y, "TEST", ctrl & 0x08);
    x = flag(x, y, "FILT", filterRouting & (1 << voice));
    if (voice3Off)
        x = text(x, y, "MUTED", SDL_Color{ 255, 90, 90, 255 });
    y += SID_ROW_HEIGHT;

    // line 2: frequency
    text(x0, y, "Freq", s_labelCol);
    std::string note = "---";
    if (hz >= 8.0)
    {
        int midi = (int)lround(69.0 + 12.0 * log2(hz / 440.0));
        if (midi >= 0)
            note = std::format("{}{}", s_noteNames[midi % 12], midi / 12 - 1);
    }
    x = text(x0 + labelW, y, std::format("${:04X}", freq), s_valueCol);
    x = text(x, y, std::format("{:8.1f} Hz", hz), s_valueCol);
    text(x, y, note, s_voiceColours[voice]);
    y += SID_ROW_HEIGHT;

    // line 3: pulse width
    text(x0, y, "Pulse", s_labelCol);
    bool pulseUsed = (ctrl & 0x40) != 0;
    x = text(x0 + labelW, y, std::format("${:03X}", pw), pulseUsed ? s_valueCol : s_offCol);
    text(x, y, std::format("{:5.1f}%", pw * 100.0 / 4096.0), pulseUsed ? s_valueCol : s_offCol);
    y += SID_ROW_HEIGHT;

    // line 4: waveform select
    text(x0, y, "Wave", s_labelCol);
    x = x0 + labelW;
    x = flag(x, y, "TRI", ctrl & 0x10);
    x = flag(x, y, "SAW", ctrl & 0x20);
    x = flag(x, y, "PULSE", ctrl & 0x40);
    x = flag(x, y, "NOISE", ctrl & 0x80);
    y += SID_ROW_HEIGHT;

    // line 5: ADSR settings
    text(x0, y, "ADSR", s_labelCol);
    x = x0 + labelW;
    x = text(x, y, std::format("A{:X} {}", attack, FormatMs(s_attackMs[attack])), s_valueCol);
    x = text(x, y, std::format("D{:X} {}", decay, FormatMs(s_attackMs[decay] * 3)), s_valueCol);
    x = text(x, y, std::format("S{:X}", sustain), s_valueCol);
    x = text(x, y, std::format("R{:X} {}", release, FormatMs(s_attackMs[release] * 3)), s_valueCol);
    y += SID_ROW_HEIGHT;

    // line 6: live envelope level
    text(x0, y, "Env", s_labelCol);
    int level = m_state.m_envelope[voice];
    SDL_FRect bar{ (float)(x0 + labelW), (float)y + 4, (float)(10 * charWidth), (float)SID_ROW_HEIGHT - 8 };
    SDL_FRect fill = bar;
    fill.w = bar.w * level / 255.0f;
    SDL_Color col = s_voiceColours[voice];
    SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, 255);
    SDL_RenderFillRect(renderer, &fill);
    SDL_SetRenderDrawColor(renderer, 90, 90, 90, 255);
    SDL_RenderRect(renderer, &bar);
    x = text(x0 + labelW + 11 * charWidth, y, std::format("{:3d}", level), s_valueCol);
    text(x, y, s_envStateNames[m_state.m_envelopeState[voice] & 3], s_labelCol);

    // waveform display fills the rest of the row
    float infoWidth = (float)(SID_INFO_CHARS * charWidth + 16);
    SDL_FRect waveArea{ area.x + infoWidth, area.y + 4, area.w - infoWidth - 8, area.h - 8 };
    DrawWaveform(renderer, voice, waveArea);
}

void SidWindow::Paint(SDL_Renderer* renderer, const Recti& dirtyArea)
{
    auto& vb = ViceBridge::Instance();
    vb.GetSidState(m_state, m_stateVersion);

    auto& wm = WindowManager::Instance();
    auto& tp = Application::Instance().GetThemeProperties();
    auto& fr = FontRenderer::Instance();
    int charWidth = fr.GetCharactorWidth(FontType::Text, '0');

    // background
    if (wm.GetActiveWindowBase() == this)
        tp.SetRenderDrawColor(renderer, ThemeColor::SourceBackgroundSelected);
    else
        tp.SetRenderDrawColor(renderer, ThemeColor::SourceBackground);
    SDL_FRect body = m_clientArea.AsSDLFRect();
    SDL_RenderFillRect(renderer, &body);

    SDL_Rect oldClip{ 0,0,0,0 };
    bool hadClip = SDL_RenderClipEnabled(renderer);
    SDL_GetRenderClipRect(renderer, &oldClip);
    auto setClip = [&](const SDL_FRect& area)
        {
            SDL_Rect clip{ (int)area.x, (int)area.y, (int)area.w, (int)area.h };
            if (hadClip)
            {
                SDL_Rect wanted = clip;
                if (!SDL_GetRectIntersection(&oldClip, &wanted, &clip))
                    clip = SDL_Rect{ 0, 0, 0, 0 };
            }
            SDL_SetRenderClipRect(renderer, &clip);
        };

    const u8* regs = m_state.m_regs;
    int cutoff = (regs[0x15] & 7) | (regs[0x16] << 3);
    int resonance = regs[0x17] >> 4;
    u8 routing = regs[0x17];
    u8 mode = regs[0x18];
    int volume = mode & 15;

    auto text = [&](int x, int y, const std::string& str, const SDL_Color& col) -> int
        {
            fr.RenderText(renderer, str, col, x, y, FontType::Text);
            return x + ((int)str.size() + 1) * charWidth;
        };
    auto flag = [&](int x, int y, const char* name, bool on) -> int
        {
            return text(x, y, name, on ? s_valueCol : s_offCol);
        };

    // header: chip + master volume
    SDL_FRect headerArea = body;
    headerArea.h = LINE_HEIGHT;
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderFillRect(renderer, &headerArea);
    setClip(headerArea);
    {
        // timebase (zoom) control
        auto& ir = IconRenderer::Instance();
        ir.DrawIcon(renderer, Icons::Zoom, m_clientArea.x + 16, m_clientArea.y + 12);
        auto& highlight = wm.GetWindowHighlightQuery();
        if (highlight.m_highlight == WindowHighlightType::EmuScreenIcon && highlight.m_window == this)
            ir.DrawIcon(renderer, Icons::Highlight, highlight.m_area.x + 8, highlight.m_area.y + 8);

        int x = (int)headerArea.x + 32;
        int y = (int)headerArea.y + 2;
        x = text(x, y, std::format("{:g}ms/scope", s_timebaseMs[m_timebase]), s_valueCol);
        x += charWidth;
        x = text(x, y, std::format("SID {}", m_state.m_model == 0 ? "6581" : "8580"), s_valueCol);
        x = text(x, y, Application::Instance().IsPAL() ? "PAL" : "NTSC", s_labelCol);
        x += charWidth;
        x = text(x, y, "Volume", s_labelCol);
        SDL_FRect bar{ (float)x, (float)y + 5, (float)(8 * charWidth), (float)SID_ROW_HEIGHT - 8 };
        SDL_FRect fill = bar;
        fill.w = bar.w * volume / 15.0f;
        SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
        SDL_RenderFillRect(renderer, &fill);
        SDL_SetRenderDrawColor(renderer, 90, 90, 90, 255);
        SDL_RenderRect(renderer, &bar);
        text(x + 9 * charWidth, y, std::format("{:2d}", volume), s_valueCol);
    }

    // filter panel at the bottom
    float filterHeight = (float)(2 * SID_ROW_HEIGHT + 8);
    SDL_FRect filterArea{ body.x, body.y + body.h - filterHeight, body.w, filterHeight };
    SDL_FRect voicesArea{ body.x, body.y + LINE_HEIGHT, body.w, body.h - LINE_HEIGHT - filterHeight };

    // voices: three equal rows, but never shorter than the text needs
    float minVoiceHeight = (float)(SID_VOICE_LINES * SID_ROW_HEIGHT + 8);
    float voiceHeight = (std::max)(voicesArea.h / 3.0f, minVoiceHeight);
    for (int v = 0; v < 3; v++)
    {
        SDL_FRect area{ voicesArea.x, voicesArea.y + v * voiceHeight, voicesArea.w, voiceHeight };
        SDL_FRect visible = area;
        if (visible.y + visible.h > voicesArea.y + voicesArea.h)
            visible.h = voicesArea.y + voicesArea.h - visible.y;
        if (visible.h <= 0.0f)
            break;

        setClip(visible);
        DrawVoice(renderer, v, area, charWidth);
        if (v > 0)
        {
            SDL_SetRenderDrawColor(renderer, 60, 60, 60, 255);
            SDL_RenderLine(renderer, area.x, area.y, area.x + area.w, area.y);
        }
    }

    setClip(filterArea);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderFillRect(renderer, &filterArea);
    SDL_SetRenderDrawColor(renderer, 80, 80, 80, 255);
    SDL_RenderLine(renderer, filterArea.x, filterArea.y, filterArea.x + filterArea.w, filterArea.y);
    {
        int x0 = (int)filterArea.x + 8;
        int y = (int)filterArea.y + 4;

        int x = text(x0, y, "Filter", s_labelCol);
        x = text(x, y, "Cutoff", s_labelCol);
        x = text(x, y, std::format("${:03X}", cutoff), s_valueCol);
        x += charWidth;
        x = text(x, y, "Resonance", s_labelCol);
        x = text(x, y, std::format("{:X}", resonance), s_valueCol);
        x += charWidth;
        x = text(x, y, "Mode", s_labelCol);
        x = flag(x, y, "LP", mode & 0x10);
        x = flag(x, y, "BP", mode & 0x20);
        x = flag(x, y, "HP", mode & 0x40);
        y += SID_ROW_HEIGHT;

        x = text(x0, y, "Routed", s_labelCol);
        x = flag(x, y, "VOICE1", routing & 0x01);
        x = flag(x, y, "VOICE2", routing & 0x02);
        x = flag(x, y, "VOICE3", routing & 0x04);
        x = flag(x, y, "EXT", routing & 0x08);
        x += charWidth;
        x = text(x, y, "Voice 3 output", s_labelCol);
        if (mode & 0x80)
            text(x, y, "OFF", SDL_Color{ 255, 90, 90, 255 });
        else
            text(x, y, "ON", s_valueCol);
    }

    SDL_SetRenderClipRect(renderer, hadClip ? &oldClip : nullptr);
}

void SidWindow::SaveTokens(std::vector<std::string>& layoutTokens)
{
    layoutTokens.push_back("SIDWINDOW");
}

bool SidWindow::CreateFromLayoutTokens(WindowLayout* layout, const std::vector<std::string>& layoutTokens, size_t& idx)
{
    if (layoutTokens[idx] != "SIDWINDOW")
        return false;

    idx++;
    layout->m_tabs.push_back(new SidWindow());
    return true;
}
