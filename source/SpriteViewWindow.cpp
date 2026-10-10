#include "common.h"
#include "SpriteViewWindow.h"
#include "WindowLayout.h"
#include "ViceBridge.h"
#include "LogManager.h"
#include "WindowManager.h"
#include "Application.h"
#include "ViceBridge.h"

#define SPRITES_X 16
#define SPRITES_Y (16*4)
#define TEX_WIDTH (SPRITES_X*24)
#define TEX_HEIGHT (SPRITES_Y*21)

SpriteViewWindow::SpriteViewWindow()
{
    m_name = "SpriteView";
}

SpriteViewWindow::~SpriteViewWindow()
{}

void SpriteViewWindow::MessageChild(WindowLayout* layout, struct WindowMessageStruct& msg)
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

bool SpriteViewWindow::Tick()
{
    m_animTime += WINDOW_TICK_MS * (1.0f / 1000.0f);
    if (m_animTime > 1.0f)
        m_animTime -= 1.0f;
    return true;
}

void SpriteViewWindow::Paint(SDL_Renderer* renderer, const Recti& dirtyArea)
{
    UpdateTexture(renderer);

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

    ViceState viceState = ViceBridge::Instance().GetViceState();
    if (m_viceTexture)
    {
        bool isPal = m_viceTextureWidth == 384;
        int lineCycles = isPal ? 63 : 65;
        float textureVScale = (float)m_viceTextureHeight;
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
}

bool SpriteViewWindow::HandleEvent(SDL_Event* e)
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
                        ViceBridge::Instance().Continue();
                        break;

                    case Icons::Pause:
                        ViceBridge::Instance().Pause();
                        break;

                    case Icons::SingleStep:
                        ViceBridge::Instance().SingleStep();
                        break;
                }
            }
        }
    }
    return false;
}

void SpriteViewWindow::SaveTokens(std::vector<std::string>& layoutTokens)
{
    layoutTokens.push_back("SPRITEVIEW");
}

void SpriteViewWindow::UpdateTexture(SDL_Renderer* renderer)
{
    if (renderer != m_renderer)
    {
        if (m_viceTexture)
            SDL_DestroyTexture(m_viceTexture);

        m_renderer = renderer;
        m_viceTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, TEX_WIDTH, TEX_HEIGHT);
        m_viceTextureWidth = TEX_WIDTH;
        m_viceTextureHeight = TEX_HEIGHT;
        SDL_SetTextureScaleMode(m_viceTexture, SDL_SCALEMODE_NEAREST);
    }

    u32* pixels = new u32[TEX_WIDTH*TEX_HEIGHT];
    u8* ram = ViceBridge::Instance().GetRam();
    for (int sy = 0; sy < SPRITES_Y; sy++)
    {
        for (int sx = 0; sx < SPRITES_X; sx++)
        {
            for (int y = 0; y < 21; y++)
            {
                for (int x = 0; x < 24; x++)
                {
                    int sprite_addr = sx * 64 + sy * 16 * 64 + x/8 + y*3;
                    int bit = ram[sprite_addr] & (1 << (7-(x & 7)));
                    int px = sx * 24 + x;
                    int py = sy * 21 + y;
                    int pixel_addr = py * TEX_WIDTH + px;
                    pixels[pixel_addr] = bit ? 0xffffffff : 0xff000000;
                }
            }

        }
    }

    if (!SDL_UpdateTexture(m_viceTexture, nullptr, pixels, TEX_WIDTH * 4))
    {
        Log(LogGroup::System, "VICE upload: {}", SDL_GetError());
    }

    delete pixels;
}

bool SpriteViewWindow::CreateFromLayoutTokens(WindowLayout* layout, const std::vector<std::string>& layoutTokens, size_t& idx)
{
    if (layoutTokens[idx] != "SPRITEVIEW")
        return false;

    idx++;
    auto win = new SpriteViewWindow();
    layout->m_tabs.push_back(win);
    return true;
}


