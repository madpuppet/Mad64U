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

bool SidWindow::Tick()
{
    return NeedsRefresh();
}

bool SidWindow::NeedsRefresh()
{
    return ViceBridge::Exists() && ViceBridge::Instance().GetSidStateVersion() != m_stateVersion;
}

//==================================================================================================================================
// Waveform display
//
// Runs the voice's oscillator (and the voice that sync / ring modulates it) over a couple of periods, starting from
// phase zero so the picture is stable, and produces the 12 bit waveform output the SID would generate.
//==================================================================================================================================
static void SimulateVoice(const SidState& sid, int voice, std::vector<float>& out)
{
    const u8* r = &sid.m_regs[voice * 7];
    int src = (voice + 2) % 3;                      // voice 1 is modulated by voice 3, 2 by 1, 3 by 2
    const u8* rs = &sid.m_regs[src * 7];

    u32 freq = r[0] | (r[1] << 8);
    u32 srcFreq = rs[0] | (rs[1] << 8);
    u32 pw = (r[2] | (r[3] << 8)) & 0xfff;
    u8 ctrl = r[4];
    bool test = (ctrl & 0x08) != 0;
    bool ring = (ctrl & 0x04) != 0;
    bool sync = (ctrl & 0x02) != 0;
    int waveSel = ctrl >> 4;

    // noise only changes 16 times per oscillator period, so show a few more periods of it
    double periods = (waveSel & 8) ? 4.0 : 2.0;
    int steps = (int)out.size();
    double inc = (freq && !test) ? periods * 16777216.0 / steps : 0.0;
    double srcInc = freq ? inc * (double)srcFreq / (double)freq : 0.0;

    double acc = 0.0, srcAcc = 0.0, total = 0.0;
    u32 shiftReg = 0x7ffff8;
    u64 noiseClock = 0;

    for (int i = 0; i < steps; i++)
    {
        u32 a = (u32)acc & 0xffffff;
        u32 sa = (u32)srcAcc & 0xffffff;

        // noise shift register is clocked each time accumulator bit 19 goes high
        u64 clock = (u64)((total + 524288.0) / 1048576.0);
        for (; noiseClock < clock; noiseClock++)
        {
            u32 bit0 = ((shiftReg >> 22) ^ (shiftReg >> 17)) & 1;
            shiftReg = ((shiftReg << 1) | bit0) & 0x7fffff;
        }

        u32 value = 0xfff;
        if (waveSel & 1)
        {
            // triangle; ring modulation replaces the oscillator MSB with (MSB xor source MSB)
            u32 msb = (ring ? (a ^ sa) : a) & 0x800000;
            value &= ((msb ? ~a : a) >> 11) & 0xfff;
        }
        if (waveSel & 2)
            value &= a >> 12;
        if (waveSel & 4)
            value &= (test || (a >> 12) >= pw) ? 0xfff : 0;
        if (waveSel & 8)
        {
            value &= ((shiftReg & 0x400000) >> 11) | ((shiftReg & 0x100000) >> 10) | ((shiftReg & 0x010000) >> 7) | ((shiftReg & 0x002000) >> 5) |
                     ((shiftReg & 0x000800) >> 4) | ((shiftReg & 0x000080) >> 1) | ((shiftReg & 0x000010) << 1) | ((shiftReg & 0x000004) << 2);
        }

        out[i] = waveSel ? ((float)value - 2048.0f) / 2048.0f : 0.0f;

        // advance; hard sync resets this oscillator when the source oscillator's MSB goes high
        double nextSrc = srcAcc + srcInc;
        bool srcMsbRise = !((u32)srcAcc & 0x800000) && ((u32)nextSrc & 0x800000);
        srcAcc = fmod(nextSrc, 16777216.0);
        acc = fmod(acc + inc, 16777216.0);
        total += inc;
        if (sync && srcMsbRise)
            acc = 0.0;
    }
}

void SidWindow::DrawWaveform(SDL_Renderer* renderer, int voice, const SDL_FRect& area)
{
    if (area.w < 16.0f || area.h < 16.0f)
        return;

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderFillRect(renderer, &area);
    SDL_SetRenderDrawColor(renderer, 60, 60, 60, 255);
    SDL_RenderRect(renderer, &area);

    float midY = area.y + area.h * 0.5f;
    SDL_RenderLine(renderer, area.x, midY, area.x + area.w, midY);

    int steps = std::clamp((int)area.w * 2, 64, 4096);
    m_wave.resize(steps);
    m_points.resize(steps);
    SimulateVoice(m_state, voice, m_wave);

    float amp = area.h * 0.5f - 3.0f;
    float level = m_state.m_envelope[voice] / 255.0f;
    SDL_Color col = s_voiceColours[voice];

    // the shape at full volume, dimmed, so it is still visible while the envelope is low or silent
    for (int i = 0; i < steps; i++)
        m_points[i] = { area.x + 1.0f + (area.w - 2.0f) * i / (steps - 1), midY - m_wave[i] * amp };
    SDL_SetRenderDrawColor(renderer, col.r / 4, col.g / 4, col.b / 4, 255);
    SDL_RenderLines(renderer, m_points.data(), steps);

    // the actual output, scaled by the live envelope level
    for (int i = 0; i < steps; i++)
        m_points[i].y = midY - m_wave[i] * amp * level;
    SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, 255);
    SDL_RenderLines(renderer, m_points.data(), steps);
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
        int x = (int)headerArea.x + 8;
        int y = (int)headerArea.y + 2;
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
