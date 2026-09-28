#include "common.h"
#include "EmuScreenWindow.h"
#include "WindowLayout.h"
#include "ViceBridge.h"
#include "LogManager.h"
#include "WindowManager.h"
#include "Application.h"

EmuScreenWindow::EmuScreenWindow()
{
    m_name = "EmuScreen";
}

EmuScreenWindow::~EmuScreenWindow()
{
}

void EmuScreenWindow::Paint(SDL_Renderer* renderer, const Recti& dirtyArea)
{
    bool fixed;
    int level;
    Application::Instance().Vice_GetZoomInfo(fixed, level);

    // draw background
    auto& tp = Application::Instance().GetThemeProperties();
    auto window = WindowManager::Instance().GetActiveWindowBase();
    if (window == this)
        tp.SetRenderDrawColor(renderer, ThemeColor::SourceBackgroundSelected);
    else
        tp.SetRenderDrawColor(renderer, ThemeColor::SourceBackground);
    SDL_FRect body = m_clientArea.AsSDLFRect();
    SDL_RenderFillRect(renderer, &body);

    if (m_viceTexture)
    {
        SDL_FRect body = m_clientArea.AsSDLFRect();
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


