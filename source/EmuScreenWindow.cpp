#include "common.h"
#include "EmuScreenWindow.h"
#include "WindowLayout.h"
#include "ViceBridge.h"
#include "LogManager.h"
#include "WindowManager.h"
#include "Application.h"
#include "ViceBridge.h"

EmuScreenWindow::EmuScreenWindow()
{
    m_name = "EmuScreen";
}

EmuScreenWindow::~EmuScreenWindow()
{
}

void EmuScreenWindow::MessageChild(WindowLayout* layout, struct WindowMessageStruct& msg)
{
    switch (msg.m_type)
    {
        case WindowMessage::Query_Highlight:
        {
            auto& fr = FontRenderer::Instance();
            auto& ir = IconRenderer::Instance();
            auto query = (WindowHighlightQuery*)msg.m_query;

            auto testIcon = [this, layout, query, &msg](Icons icon, int x, int y) -> bool
                {
                    Recti area = IconRenderer::Instance().CalcIconArea(icon, x, y);
                    if (area.Contains(msg.m_x, msg.m_y))
                    {
                        query->m_area = area;
                        query->m_highlight = WindowHighlightType::EmuScreenIcon;
                        query->m_id = 0;
                        query->m_emuScreen.m_icon = icon;
                        query->m_tree = msg.m_tree;
                        query->m_layout = layout;
                        query->m_window = this;
                        msg.m_response++;
                        return true;
                    }
                    return false;
                };

            if (testIcon(Icons::Run, m_clientArea.x + 16, m_clientArea.y + 12))
                return;
            if (testIcon(Icons::Pause, m_clientArea.x + 36, m_clientArea.y + 12))
                return;
            if (testIcon(Icons::SingleStep, m_clientArea.x + 56, m_clientArea.y + 12))
                return;

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

bool EmuScreenWindow::Tick()
{
    m_animTime += WINDOW_TICK_MS * (1.0f / 1000.0f);
    if (m_animTime > 1.0f)
        m_animTime -= 1.0f;
    return true;
}

void EmuScreenWindow::Paint(SDL_Renderer* renderer, const Recti& dirtyArea)
{
    bool fixed;
    int level;
    Application::Instance().Vice_GetZoomInfo(fixed, level);
    auto& wm = WindowManager::Instance();
    auto& highlight = wm.GetWindowHighlightQuery();

    // draw background
    auto& tp = Application::Instance().GetThemeProperties();
    auto window = WindowManager::Instance().GetActiveWindowBase();
    if (window == this)
        tp.SetRenderDrawColor(renderer, ThemeColor::SourceBackgroundSelected);
    else
        tp.SetRenderDrawColor(renderer, ThemeColor::SourceBackground);

    SDL_FRect body = m_clientArea.AsSDLFRect();
    body.y += LINE_HEIGHT;
    SDL_RenderFillRect(renderer, &body);

    auto& viceState = gViceBridge->GetViceState();
    if (m_viceTexture)
    {
        bool isPal = m_viceTextureWidth == 384;
        int lineCycles = isPal ? 63 : 65;
        float textureVScale = isPal ? (float)m_viceTextureHeight * 1.07f : (float)m_viceTextureHeight * 0.75f;

        if (!fixed)
        {
            float frameAR = (float)m_viceTextureWidth / textureVScale;
            float clientAreaAR = body.w / body.h;
            if (clientAreaAR > frameAR)
            {
                // widescreen - use full height, scale width
                body.w = body.h * frameAR;
            }
            else
            {
                // tallscreen - use full width
                body.h = body.w / frameAR;
            }
            SDL_RenderTexture(renderer, m_viceTexture, nullptr, &body);
        }
        else
        {
            float zoom = (float)level;
            body.w = m_viceTextureWidth * zoom;
            body.h = textureVScale * zoom;
            body.x -= m_clientContentOffset.x;
            body.y -= m_clientContentOffset.y;
            SDL_RenderTexture(renderer, m_viceTexture, nullptr, &body);
        }

        int rasterX = (viceState.m_rasterCycle - 14) * 8;
        if (rasterX >= 0 && rasterX < 384 && viceState.m_rasterLine >= 16)
        {
            float pixelWidth = body.w / m_viceTextureWidth;
            float pixelHeight = body.h / m_viceTextureHeight;

            SDL_FRect cycleArea{ body.x + rasterX * pixelWidth, body.y + pixelHeight * (viceState.m_rasterLine - 16), pixelWidth * 8.0f, pixelHeight };
            SDL_SetRenderDrawColor(m_renderer, 255, 255, 0, 128);
            SDL_RenderRect(m_renderer, &cycleArea);
        }

        m_clientContentSize.x = (int)body.w;
        m_clientContentSize.y = (int)body.h;
        LayoutScrollbars();
    }

    // draw icons
    auto& ir = IconRenderer::Instance();

    SDL_FRect headerArea = m_clientArea.AsSDLFRect();
    headerArea.h = LINE_HEIGHT;
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderFillRect(renderer, &headerArea);
    ir.DrawIcon(renderer, Icons::Run, m_clientArea.x + 16, m_clientArea.y + 12);
    ir.DrawIcon(renderer, Icons::Pause, m_clientArea.x + 36, m_clientArea.y + 12);
    ir.DrawIcon(renderer, Icons::SingleStep, m_clientArea.x + 56, m_clientArea.y + 12);

    if (highlight.m_highlight == WindowHighlightType::EmuScreenIcon)
    {
        int ix = highlight.m_area.x + 8;
        int iy = highlight.m_area.y + 8;
        ir.DrawIcon(renderer, Icons::Highlight, ix, iy);
    }

    // draw emulator state
    auto& fr = FontRenderer::Instance();
    SDL_Color stateCol{ 255,255,255,255 };
    std::string text = std::format("PC {:4x} ROW {:3d} COL {:3d} A {:2x} X {:2x} Y {:2x} Flags {}{}.{}{}{}{}{}",
        viceState.m_pc, viceState.m_rasterLine, viceState.m_rasterCycle, viceState.m_acc, viceState.m_x, viceState.m_y,
        viceState.m_flags & 128 ? 'N' : 'n',
        viceState.m_flags & 64 ? 'V' : 'v',
        viceState.m_flags & 16 ? 'B' : 'b',
        viceState.m_flags & 8 ? 'D' : 'd',
        viceState.m_flags & 4 ? 'I' : 'i',
        viceState.m_flags & 2 ? 'Z' : 'z',
        viceState.m_flags & 1 ? 'C' : 'c');
    fr.RenderText(renderer, text, stateCol, m_clientArea.x + 100, m_clientArea.y, FontType::UI);
}

bool EmuScreenWindow::HandleEvent(SDL_Event* e)
{
    switch (e->type)
    {
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        {
            auto& selected = WindowManager::Instance().GetWindowSelectionQuery();
            if (selected.m_highlight == WindowHighlightType::EmuScreenIcon && selected.m_window == this)
            {
                switch (selected.m_emuScreen.m_icon)
                {
                    case Icons::Run:
                        gViceBridge->Continue();
                        break;

                    case Icons::Pause:
                        gViceBridge->Pause();
                        break;

                    case Icons::SingleStep:
                        gViceBridge->SingleStep();
                        break;
                }
            }
        }
    }
    return false;
}

void EmuScreenWindow::SaveTokens(std::vector<std::string>& layoutTokens)
{
    layoutTokens.push_back("EMUSCREEN");
}

void EmuScreenWindow::UpdateTexture(SDL_Renderer* renderer, struct ViceFrame& frame)
{
    if (renderer != m_renderer || !m_viceTexture || m_viceTextureWidth != frame.m_width || m_viceTextureHeight != frame.m_height)
    {
        if (m_viceTexture)
            SDL_DestroyTexture(m_viceTexture);

        m_renderer = renderer;
        m_viceTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, frame.m_width, frame.m_height);
        m_viceTextureWidth = frame.m_width;
        m_viceTextureHeight = frame.m_height;

        SDL_SetTextureScaleMode(m_viceTexture, SDL_SCALEMODE_NEAREST);
    }

    if (!SDL_UpdateTexture(m_viceTexture, nullptr, frame.m_pixels, frame.m_width * 4))
    {
        Log(LogGroup::System, "VICE upload: {}", SDL_GetError());
    }
}

bool EmuScreenWindow::CreateFromLayoutTokens(WindowLayout* layout, const std::vector<std::string>& layoutTokens, size_t& idx)
{
    if (layoutTokens[idx] != "EMUSCREEN")
        return false;

    idx++;
    auto win = new EmuScreenWindow();
    layout->m_tabs.push_back(win);
    return true;
}


