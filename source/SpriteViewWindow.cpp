#include "common.h"
#include "SpriteViewWindow.h"
#include "WindowLayout.h"
#include "ViceBridge.h"
#include "LogManager.h"
#include "WindowManager.h"
#include "Application.h"
#include "ViceBridge.h"
#include "Settings.h"

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

            if (testIcon(Icons::Zoom, m_clientArea.x + 16, m_clientArea.y + 12))
                return;
            if (testIcon(Icons::Multicolour, m_clientArea.x + 36, m_clientArea.y + 12))
                return;
            for (int i = 0; i < 4; i++)
            {
                if (testIcon(Icons::ColourBall, m_clientArea.x + 56 + i * 20, m_clientArea.y + 12))
                {
                    query->m_id = i;
                    return;
                }
            }

            if (m_clientArea.Contains(msg.m_x, msg.m_y))
            {
                Recti spriteArea = CalcSpriteRenderArea();
                int spriteIdx = CursorToSpriteIdx(msg.m_x, msg.m_y);
                query->m_area = SpriteIdxToArea(spriteIdx);
                query->m_highlight = WindowHighlightType::SpriteArea;
                query->m_id = spriteIdx;
                query->m_tree = msg.m_tree;
                query->m_layout = layout;
                query->m_window = this;
                msg.m_response++;
                return;
            }

            msg.m_response++;
            query->m_area = m_clientArea;
            query->m_highlight = WindowHighlightType::ClientArea;
            query->m_tree = msg.m_tree;
            query->m_layout = layout;
            query->m_window = this;
            return;
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

static const char* s_zoomText[] = { "Auto", "1x", "2x", "4x", "8x" };
void SpriteViewWindow::Paint(SDL_Renderer* renderer, const Recti& dirtyArea)
{
    UpdateTexture(renderer);

    bool fixed = (m_zoom != 0);
    int level = 1<<(m_zoom-1);

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
    body.h -= LINE_HEIGHT;
    SDL_RenderFillRect(renderer, &body);

    ViceState viceState = ViceBridge::Instance().GetViceState();
    if (m_viceTexture)
    {
        if (!fixed)
        {
            body.h = body.w / m_viceTextureWidth * m_viceTextureHeight;
            body.x -= m_clientContentOffset.x;
            body.y -= m_clientContentOffset.y;
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

    // draw icons
    auto& ir = IconRenderer::Instance();

    SDL_FRect headerArea = m_clientArea.AsSDLFRect();
    headerArea.h = LINE_HEIGHT;
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderFillRect(renderer, &headerArea);
    ir.DrawIcon(renderer, Icons::Zoom, m_clientArea.x + 16, m_clientArea.y + 12);
    ir.DrawIcon(renderer, Icons::Multicolour, m_clientArea.x + 36, m_clientArea.y + 12);
    for (int i = 0; i < 4; i++)
    {
        ir.SetIconColor(renderer, Icons::ColourBall, C64Palette[ViewColours[i]]);
        ir.DrawIcon(renderer, Icons::ColourBall, m_clientArea.x + 56 + i * 20, m_clientArea.y + 12);
    }

    // draw emulator state
    auto& fr = FontRenderer::Instance();
    SDL_Color stateCol{ 255,255,255,255 };
    std::string isMulti = m_multicolour ? "Multicol" : "Hires";
    std::string text = std::format("{} Zoom {}", isMulti, s_zoomText[m_zoom]);
    fr.RenderText(renderer, text, stateCol, m_clientArea.x + 160, m_clientArea.y, FontType::UI);

    // draw highlight
    if (highlight.m_highlight == WindowHighlightType::EmuScreenIcon)
    {
        int ix = highlight.m_area.x + 8;
        int iy = highlight.m_area.y + 8;
        ir.DrawIcon(renderer, Icons::Highlight, ix, iy);
    }
    else if (highlight.m_highlight == WindowHighlightType::SpriteArea)
    {
        SDL_FRect rect = highlight.m_area.AsSDLFRect();
        SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
        SDL_RenderRect(renderer, &rect);

        SDL_Color addrCol = tp.Col(ThemeColor::TextOperator);
        SDL_Color sprCol = tp.Col(ThemeColor::TextString);
        SDL_FRect textRect = { rect.x, rect.y + 10.0f + rect.h, rect.w, LINE_HEIGHT * 2 };
        std::string addrStr = std::format("{:04x}", highlight.m_id * 64);
        std::string sprStr = std::format("{:1x}:{:02x}", highlight.m_id / 256, highlight.m_id & 255);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 200);
        SDL_RenderFillRect(renderer, &textRect);
        fr.RenderTextCentered(renderer, addrStr, addrCol, (int)(textRect.x + textRect.w/2), (int)(textRect.y + LINE_HEIGHT), FontType::UI);
        fr.RenderTextCentered(renderer, sprStr, sprCol, (int)(textRect.x + textRect.w/2), (int)(textRect.y + LINE_HEIGHT*2), FontType::UI);
    }
}

Recti SpriteViewWindow::CalcSpriteRenderArea()
{
    Recti area;

    bool fixed = (m_zoom != 0);
    int level = 1 << (m_zoom - 1);

    if (!fixed)
    {
        area.w = m_clientArea.w;
        area.h = area.w * m_viceTextureHeight / m_viceTextureWidth;
    }
    else
    {
        area.w = m_viceTextureWidth * level;
        area.h = m_viceTextureHeight * level;
    }

    area.x = m_clientArea.x - m_clientContentOffset.x;
    area.y = m_clientArea.y + LINE_HEIGHT - m_clientContentOffset.y;

    return area;
}

Recti SpriteViewWindow::SpriteIdxToArea(int idx)
{
    Recti area;
    Recti texArea = CalcSpriteRenderArea();
    int col = idx % SPRITES_X;
    int row = idx / SPRITES_X;

    area.x = texArea.x + texArea.w * col / SPRITES_X;
    area.y = texArea.y + texArea.h * row / SPRITES_Y;
    area.w = texArea.w / SPRITES_X;
    area.h = texArea.h / SPRITES_Y;
    return area;
}

int SpriteViewWindow::CursorToSpriteIdx(int x, int y)
{
    if (!m_viceTexture)
        return 0;

    Recti area = CalcSpriteRenderArea();
    float sprite_height = (float)area.h / (float)SPRITES_Y;
    float sprite_width = (float)area.w / (float)SPRITES_X;
    int row = (int)((y - (m_clientArea.y + LINE_HEIGHT) + m_clientContentOffset.y) / sprite_height);
    int col = (int)((x - (m_clientArea.x) + m_clientContentOffset.x) / sprite_width);
    return row * SPRITES_X + col;
}

bool SpriteViewWindow::HandleEvent(SDL_Event* e)
{
    switch (e->type)
    {
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        {
            if (e->button.button == 1)
            {
                auto& selected = WindowManager::Instance().GetWindowSelectionQuery();
                if (selected.m_highlight == WindowHighlightType::EmuScreenIcon && selected.m_window == this)
                {
                    switch (selected.m_emuScreen.m_icon)
                    {
                        case Icons::Zoom:
                            m_zoom = (m_zoom + 1) % 5;
                            break;

                        case Icons::Multicolour:
                            m_multicolour = !m_multicolour;
                            break;
                    
                        case Icons::ColourBall:
                            ViewColours[selected.m_id] = (ViewColours[selected.m_id] + 1) & 15;
                            WriteViewColourSettings();
                            break;
                    }
                    return true;
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

static u32 s_col[4] = { 0xff000000, 0xffff0000, 0xff00ff00, 0xff0000ff };
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
    
    if (m_multicolour)
    {
        for (int sy = 0; sy < SPRITES_Y; sy++)
        {
            for (int sx = 0; sx < SPRITES_X; sx++)
            {
                for (int y = 0; y < 21; y++)
                {
                    for (int x = 0; x < 12; x++)
                    {
                        int sprite_addr = sx * 64 + sy * 16 * 64 + x / 4 + y * 3;
                        int bit = (ram[sprite_addr] >> (6 - ((x & 3)*2))) & 3;
                        int px = sx * 24 + x*2;
                        int py = sy * 21 + y;
                        int pixel_addr = py * TEX_WIDTH + px;
                        pixels[pixel_addr] = C64Palette[ViewColours[bit]];
                        pixels[pixel_addr+1] = C64Palette[ViewColours[bit]];
                    }
                }
            }
        }
    }
    else
    {
        for (int sy = 0; sy < SPRITES_Y; sy++)
        {
            for (int sx = 0; sx < SPRITES_X; sx++)
            {
                for (int y = 0; y < 21; y++)
                {
                    for (int x = 0; x < 24; x++)
                    {
                        int sprite_addr = sx * 64 + sy * 16 * 64 + x / 8 + y * 3;
                        int bit = ram[sprite_addr] & (1 << (7 - (x & 7)));
                        int px = sx * 24 + x;
                        int py = sy * 21 + y;
                        int pixel_addr = py * TEX_WIDTH + px;
                        pixels[pixel_addr] = bit ? 0xffffffff : 0xff000000;
                    }
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


