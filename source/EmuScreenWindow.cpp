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

void EmuScreenWindow::Paint(SDL_Renderer* renderer, const Recti& dirtyArea)
{
    bool fixed;
    int level;
    Application::Instance().Vice_GetZoomInfo(fixed, level);
    auto& wm = WindowManager::Instance();
    auto& highlight = wm.GetWindowHighlightQuery();

    // draw icons
    auto& ir = IconRenderer::Instance();
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
    auto& viceState = gViceBridge->GetViceState();
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

    // draw background
    auto& tp = Application::Instance().GetThemeProperties();
    auto window = WindowManager::Instance().GetActiveWindowBase();
    if (window == this)
        tp.SetRenderDrawColor(renderer, ThemeColor::SourceBackgroundSelected);
    else
        tp.SetRenderDrawColor(renderer, ThemeColor::SourceBackground);

    SDL_FRect body = m_clientArea.AsSDLFRect();
    body.x = (float)m_clientArea.x;
    body.y = (float)(m_clientArea.y + LINE_HEIGHT);
    body.w = (float)m_clientArea.w;
    body.h = (float)m_clientArea.h - LINE_HEIGHT;
    SDL_RenderFillRect(renderer, &body);

    if (m_viceTexture)
    {
        if (!fixed)
        {
            float frameAR = (float)m_viceTextureWidth / (float)m_viceTextureHeight;
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
            body.h = m_viceTextureHeight * zoom;
            body.x -= m_clientContentOffset.x;
            body.y -= m_clientContentOffset.y;
            SDL_RenderTexture(renderer, m_viceTexture, nullptr, &body);
        }
        m_clientContentSize.x = (int)body.w;
        m_clientContentSize.y = (int)body.h;
        LayoutScrollbars();
    }
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


