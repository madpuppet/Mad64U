#include "common.h"
#include "VicScreenWindow.h"
#include "WindowLayout.h"
#include "ViceBridge.h"
#include "LogManager.h"
#include "WindowManager.h"
#include "Application.h"

// start of the 40 column display window inside a raster line (dbuf coords) and its first/last lines
#define VIC_DISPLAY_X 136
#define VIC_DISPLAY_W 320
#define VIC_DISPLAY_Y 51
#define VIC_DISPLAY_H 200

// sprite X coordinate -> raster (dbuf) pixel: sprite X 24 lines up with the left edge of the display window
#define VIC_SPRITE_X_OFFSET (VIC_DISPLAY_X - 24)

#define VIC_REG_COUNT 0x2f
#define REG_ROW_HEIGHT 18
#define REG_COL_CHARS 26
#define INFO_ROWS 3

enum class VicRegDecode { None, SpriteX, SpriteY, Bits, Colour, RasterRead };
struct VicRegInfo
{
    const char* m_name;
    VicRegDecode m_decode;
};
static const VicRegInfo s_vicRegs[VIC_REG_COUNT] = {
    { "S0X", VicRegDecode::SpriteX }, { "S0Y", VicRegDecode::SpriteY },
    { "S1X", VicRegDecode::SpriteX }, { "S1Y", VicRegDecode::SpriteY },
    { "S2X", VicRegDecode::SpriteX }, { "S2Y", VicRegDecode::SpriteY },
    { "S3X", VicRegDecode::SpriteX }, { "S3Y", VicRegDecode::SpriteY },
    { "S4X", VicRegDecode::SpriteX }, { "S4Y", VicRegDecode::SpriteY },
    { "S5X", VicRegDecode::SpriteX }, { "S5Y", VicRegDecode::SpriteY },
    { "S6X", VicRegDecode::SpriteX }, { "S6Y", VicRegDecode::SpriteY },
    { "S7X", VicRegDecode::SpriteX }, { "S7Y", VicRegDecode::SpriteY },
    { "SXMSB", VicRegDecode::Bits },
    { "CTRL1", VicRegDecode::Bits },
    { "RASTER", VicRegDecode::RasterRead },
    { "LPENX", VicRegDecode::None },
    { "LPENY", VicRegDecode::None },
    { "SPREN", VicRegDecode::Bits },
    { "CTRL2", VicRegDecode::Bits },
    { "SPRYEX", VicRegDecode::Bits },
    { "MEMPTR", VicRegDecode::None },
    { "IRQ", VicRegDecode::Bits },
    { "IRQEN", VicRegDecode::Bits },
    { "SPRPRI", VicRegDecode::Bits },
    { "SPRMC", VicRegDecode::Bits },
    { "SPRXEX", VicRegDecode::Bits },
    { "SPRSPR", VicRegDecode::Bits },
    { "SPRBG", VicRegDecode::Bits },
    { "BORDER", VicRegDecode::Colour },
    { "BG0", VicRegDecode::Colour },
    { "BG1", VicRegDecode::Colour },
    { "BG2", VicRegDecode::Colour },
    { "BG3", VicRegDecode::Colour },
    { "SPRMC0", VicRegDecode::Colour },
    { "SPRMC1", VicRegDecode::Colour },
    { "S0COL", VicRegDecode::Colour },
    { "S1COL", VicRegDecode::Colour },
    { "S2COL", VicRegDecode::Colour },
    { "S3COL", VicRegDecode::Colour },
    { "S4COL", VicRegDecode::Colour },
    { "S5COL", VicRegDecode::Colour },
    { "S6COL", VicRegDecode::Colour },
    { "S7COL", VicRegDecode::Colour },
};

static SDL_Color ToSDLColor(u32 argb)
{
    return SDL_Color{ (u8)(argb >> 16), (u8)(argb >> 8), (u8)argb, 255 };
}

VicScreenWindow::VicScreenWindow()
{
    m_name = "VicScreen";
}

VicScreenWindow::~VicScreenWindow()
{
    // stop the emulation thread capturing snapshots (and remove any raster breakpoint)
    if (m_captureStarted && ViceBridge::Exists())
        ViceBridge::Instance().SetRasterPoint(VicRasterPointMode::Off);

    if (m_texture)
        SDL_DestroyTexture(m_texture);
}

void VicScreenWindow::MessageChild(WindowLayout* layout, struct WindowMessageStruct& msg)
{
    switch (msg.m_type)
    {
        case WindowMessage::Query_Highlight:
        {
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

            m_mouseX = -1;
            m_mouseY = -1;

            if (testIcon(Icons::Run, m_clientArea.x + 16, m_clientArea.y + 12))
                return;
            if (testIcon(Icons::Pause, m_clientArea.x + 36, m_clientArea.y + 12))
                return;
            if (testIcon(Icons::SingleStep, m_clientArea.x + 56, m_clientArea.y + 12))
                return;
            if (testIcon(Icons::Zoom, m_clientArea.x + 76, m_clientArea.y + 12))
                return;

            if (m_clientArea.Contains(msg.m_x, msg.m_y))
            {
                m_mouseX = msg.m_x;
                m_mouseY = msg.m_y;

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

bool VicScreenWindow::Tick()
{
    // the beam moves whenever the emulator runs or steps, so always repaint
    return true;
}

//==================================================================================================================================
// Static VIC-II frame renderer
//
// Renders a complete frame (border, graphics and sprites) as colour indices, treating the registers and all memory in the
// snapshot as constant for the whole frame.  No raster effects: what you see is what the VIC would show if nothing
// changed from this instant on.
//
// Coordinates: x is in pixels from the start of the raster line (8 per cycle), y is the raster line.
//==================================================================================================================================


static int VicGraphicsMode(const u8* regs)
{
    return ((regs[0x11] & 0x40) ? 4 : 0) | ((regs[0x11] & 0x20) ? 2 : 0) | ((regs[0x16] & 0x10) ? 1 : 0);
}

static void RenderVicFrame(const VicRasterState& vic, u8* out, int width, int height)
{
    const u8* regs = vic.m_regs;

    // everything starts as border; the display window is cut out of it below
    memset(out, regs[0x20] & 15, (size_t)width * height);

    // DEN off: the border never opens, so the whole frame is border colour
    if (!(regs[0x11] & 0x10))
        return;

    bool rsel = (regs[0x11] & 0x08) != 0;       // 25 rows
    bool csel = (regs[0x16] & 0x08) != 0;       // 40 columns
    int yscroll = regs[0x11] & 7;
    int xscroll = regs[0x16] & 7;
    int mode = VicGraphicsMode(regs);
    bool ecm = (mode & 4) != 0;

    // area not covered by the border
    int winTop = rsel ? VIC_DISPLAY_Y : VIC_DISPLAY_Y + 4;
    int winBottom = rsel ? VIC_DISPLAY_Y + 200 : VIC_DISPLAY_Y + 196;
    int winLeft = csel ? VIC_DISPLAY_X : VIC_DISPLAY_X + 7;
    int winRight = csel ? VIC_DISPLAY_X + 320 : VIC_DISPLAY_X + 311;
    winBottom = (std::min)(winBottom, height);
    winRight = (std::min)(winRight, width);

    int screenBase = (regs[0x18] >> 4) * 0x400;
    int charBase = ((regs[0x18] >> 1) & 7) * 0x800;
    int bitmapBase = (regs[0x18] & 0x08) ? 0x2000 : 0;

    u8 bg[4] = { (u8)(regs[0x21] & 15), (u8)(regs[0x22] & 15), (u8)(regs[0x23] & 15), (u8)(regs[0x24] & 15) };

    // per pixel "foreground" flag for the display window, used for sprite priority
    static u8 foreground[VIC_RASTER_MAX_WIDTH * VIC_RASTER_MAX_LINES];

    //------------------------------------------------------------------------------------------------------------------
    // graphics
    //------------------------------------------------------------------------------------------------------------------
    for (int y = winTop; y < winBottom; y++)
    {
        u8* outLine = out + y * width;
        u8* fgLine = foreground + y * width;

        // the first character row starts on the first bad line: line $30 + yscroll
        int rel = y - (0x30 + yscroll);
        bool idle = (rel < 0 || rel >= 200);
        int row = idle ? 0 : rel >> 3;
        int rc = idle ? 0 : rel & 7;

        // in idle state the VIC displays the byte at $3fff ($39ff in ECM) with all colour data zero
        u8 idleData = vic.VicRead(ecm ? 0x39ff : 0x3fff);

        int lastCol = -1;
        u8 data = 0, scr = 0, colr = 0;

        for (int x = winLeft; x < winRight; x++)
        {
            int gx = x - VIC_DISPLAY_X - xscroll;
            if (gx < 0 || gx >= 320)
            {
                // pixels scrolled in from outside the 40 columns show the background
                outLine[x] = bg[0];
                fgLine[x] = 0;
                continue;
            }

            int col = gx >> 3;
            if (col != lastCol)
            {
                lastCol = col;
                if (idle)
                {
                    data = idleData;
                    scr = 0;
                    colr = 0;
                }
                else
                {
                    int cell = row * 40 + col;
                    scr = vic.VicRead(screenBase + cell);
                    colr = vic.m_colourRam[cell] & 15;
                    if (mode & 2)
                        data = vic.VicRead(bitmapBase + cell * 8 + rc);
                    else
                        data = vic.VicRead(charBase + (ecm ? (scr & 0x3f) : scr) * 8 + rc);
                }
            }

            int bit = 7 - (gx & 7);
            bool hires = ((data >> bit) & 1) != 0;
            int pair = (data >> (bit & 6)) & 3;
            u8 c = 0;
            bool fg = false;

            switch (mode)
            {
                case 0: // standard text
                    fg = hires;
                    c = hires ? colr : bg[0];
                    break;

                case 1: // multicolour text - per character, selected by bit 3 of the colour nibble
                    if (colr & 8)
                    {
                        fg = pair >= 2;
                        c = (pair == 3) ? (colr & 7) : bg[pair];
                    }
                    else
                    {
                        fg = hires;
                        c = hires ? (colr & 7) : bg[0];
                    }
                    break;

                case 2: // hires bitmap
                    fg = hires;
                    c = hires ? (scr >> 4) : (scr & 15);
                    break;

                case 3: // multicolour bitmap
                    fg = pair >= 2;
                    switch (pair)
                    {
                        case 0: c = bg[0]; break;
                        case 1: c = scr >> 4; break;
                        case 2: c = scr & 15; break;
                        case 3: c = colr; break;
                    }
                    break;

                case 4: // extended colour text - top 2 bits of the screen code select the background
                    fg = hires;
                    c = hires ? colr : bg[scr >> 6];
                    break;

                default: // invalid modes display black, but still generate foreground for sprite priority
                    fg = (mode & 1) ? (pair >= 2) : hires;
                    c = 0;
                    break;
            }

            outLine[x] = c;
            fgLine[x] = fg ? 1 : 0;
        }
    }

    //------------------------------------------------------------------------------------------------------------------
    // sprites - drawn 7 to 0 so lower numbered sprites end up on top.  Only visible where the border is open.
    //------------------------------------------------------------------------------------------------------------------
    static u8 spriteColour[VIC_RASTER_MAX_WIDTH * VIC_RASTER_MAX_LINES];    // 0xff = no sprite pixel
    static u8 spriteBehind[VIC_RASTER_MAX_WIDTH * VIC_RASTER_MAX_LINES];
    if (!regs[0x15])
        return;
    memset(spriteColour, 0xff, (size_t)width * height);

    for (int i = 7; i >= 0; i--)
    {
        int mask = 1 << i;
        if (!(regs[0x15] & mask))
            continue;

        int sx = regs[i * 2] | ((regs[0x10] & mask) ? 0x100 : 0);
        int sy = regs[i * 2 + 1];
        bool xexp = (regs[0x1d] & mask) != 0;
        bool yexp = (regs[0x17] & mask) != 0;
        bool multi = (regs[0x1c] & mask) != 0;
        u8 behind = (regs[0x1b] & mask) ? 1 : 0;
        u8 sprCol = regs[0x27 + i] & 15;
        u8 mc0 = regs[0x25] & 15;
        u8 mc1 = regs[0x26] & 15;

        int dataBase = vic.VicRead(screenBase + 0x3f8 + i) * 64;
        int sprW = xexp ? 48 : 24;
        int sprH = yexp ? 42 : 21;

        for (int dy = 0; dy < sprH; dy++)
        {
            // the sprite is displayed from the line after its Y compare matches
            int y = (sy + 1 + dy) % height;
            if (y < winTop || y >= winBottom)
                continue;

            int srow = yexp ? dy >> 1 : dy;
            u32 bits = (vic.VicRead(dataBase + srow * 3) << 16) | (vic.VicRead(dataBase + srow * 3 + 1) << 8) | vic.VicRead(dataBase + srow * 3 + 2);

            for (int dx = 0; dx < sprW; dx++)
            {
                int x = (sx + VIC_SPRITE_X_OFFSET + dx) % width;
                if (x < winLeft || x >= winRight)
                    continue;

                int p = xexp ? dx >> 1 : dx;
                u8 c;
                if (multi)
                {
                    int v = (bits >> (22 - (p & ~1))) & 3;
                    if (v == 0)
                        continue;
                    c = (v == 1) ? mc0 : (v == 2) ? sprCol : mc1;
                }
                else
                {
                    if (!((bits >> (23 - p)) & 1))
                        continue;
                    c = sprCol;
                }

                spriteColour[y * width + x] = c;
                spriteBehind[y * width + x] = behind;
            }
        }
    }

    // combine: the topmost sprite pixel wins, and is hidden by foreground graphics if its priority bit is set
    for (int y = winTop; y < winBottom; y++)
    {
        for (int x = winLeft; x < winRight; x++)
        {
            int idx = y * width + x;
            u8 c = spriteColour[idx];
            if (c != 0xff && !(spriteBehind[idx] && foreground[idx]))
                out[idx] = c;
        }
    }
}

bool VicScreenWindow::NeedsRefresh()
{
    auto& vb = ViceBridge::Instance();
    if (!m_captureStarted || vb.HasViceStopped() || m_pointMode == VicRasterPointMode::Break)
        return false;
    return vb.GetVicSnapshotVersion() != m_snapshotVersion;
}

void VicScreenWindow::UpdateTexture(SDL_Renderer* renderer)
{
    // When to take a new snapshot of the VIC state:
    //  - emulator stopped: always, so the display shows the state at the stop
    //  - running to a raster breakpoint (red): never, hold the display until it is hit
    //  - otherwise: when the beam crosses the refresh point (green), or the top of the frame if there is no point
    auto& vb = ViceBridge::Instance();

    // windows restored from the saved layout are constructed before the ViceBridge exists,
    // so start the top-of-frame snapshot capture on first paint rather than in the constructor
    if (!m_captureStarted)
    {
        m_captureStarted = true;
        vb.SetRasterPoint(VicRasterPointMode::TopOfFrame);
    }

    bool changed = true;
    if (vb.HasViceStopped())
        vb.GetVicRasterState(m_raster);
    else if (m_pointMode != VicRasterPointMode::Break)
        changed = vb.GetVicSnapshot(m_raster, m_snapshotVersion);
    else
        changed = false;

    int width = m_raster.Width();
    int height = m_raster.m_lineCount;

    // nothing new to show: keep the texture we already have
    if (!changed && m_texture && renderer == m_renderer && m_textureWidth == width && m_textureHeight == height)
        return;

    if (renderer != m_renderer || !m_texture || m_textureWidth != width || m_textureHeight != height)
    {
        if (m_texture)
            SDL_DestroyTexture(m_texture);

        m_renderer = renderer;
        m_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, width, height);
        m_textureWidth = width;
        m_textureHeight = height;
        if (!m_texture)
        {
            Log(LogGroup::System, "VicScreen texture: {}", SDL_GetError());
            return;
        }
        SDL_SetTextureScaleMode(m_texture, SDL_SCALEMODE_NEAREST);
    }

    // render the whole frame from the snapshot
    static u8 frame[VIC_RASTER_MAX_WIDTH * VIC_RASTER_MAX_LINES];
    RenderVicFrame(m_raster, frame, width, height);

    void* texPixels = nullptr;
    int pitch = 0;
    if (!SDL_LockTexture(m_texture, nullptr, &texPixels, &pitch))
    {
        Log(LogGroup::System, "VicScreen lock: {}", SDL_GetError());
        return;
    }

    for (int y = 0; y < height; y++)
    {
        u32* out = (u32*)((u8*)texPixels + y * pitch);
        const u8* in = frame + y * width;
        for (int x = 0; x < width; x++)
            out[x] = C64Palette[in[x] & 15];
    }

    SDL_UnlockTexture(m_texture);
}

static const char* s_zoomText[] = { "Auto", "1x", "2x", "4x" };
void VicScreenWindow::Paint(SDL_Renderer* renderer, const Recti& dirtyArea)
{
    UpdateTexture(renderer);

    auto& wm = WindowManager::Instance();
    auto& highlight = wm.GetWindowHighlightQuery();

    // draw background
    auto& tp = Application::Instance().GetThemeProperties();
    if (wm.GetActiveWindowBase() == this)
        tp.SetRenderDrawColor(renderer, ThemeColor::SourceBackgroundSelected);
    else
        tp.SetRenderDrawColor(renderer, ThemeColor::SourceBackground);

    SDL_FRect body = m_clientArea.AsSDLFRect();
    body.y += LINE_HEIGHT;
    body.h -= LINE_HEIGHT;
    SDL_RenderFillRect(renderer, &body);

    // register panel on the right; flows into extra columns if the window isn't tall enough
    auto& fr = FontRenderer::Instance();
    int charWidth = fr.GetCharactorWidth(FontType::Text, '0');
    int panelRows = (std::max)(1, ((int)body.h - 8) / REG_ROW_HEIGHT);
    int panelCols = (VIC_REG_COUNT + panelRows - 1) / panelRows;
    panelRows = (VIC_REG_COUNT + panelCols - 1) / panelCols;
    int panelColWidth = REG_COL_CHARS * charWidth + 8;
    float panelWidth = (float)(panelCols * panelColWidth + 8);
    SDL_FRect panelArea{ body.x + body.w - panelWidth, body.y, panelWidth, body.h };
    body.w = (std::max)(body.w - panelWidth, 16.0f);

    // info panel below the visualisation
    float infoHeight = (float)(INFO_ROWS * REG_ROW_HEIGHT + 8);
    SDL_FRect infoArea{ body.x, body.y + body.h - infoHeight, body.w, infoHeight };
    body.h = (std::max)(body.h - infoHeight, 16.0f);

    bool isPal = m_raster.m_lineCount > 300;
    int hoverLine = -1;
    int hoverCycle = -1;
    int hoverX = -1;

    if (m_texture)
    {
        // match EmuScreen's pixel aspect correction
        float textureVScale = isPal ? (float)m_textureHeight * 1.07f : (float)m_textureHeight * 1.07f * 271.0f / 246.0f;

        if (m_zoom == 0)
        {
            float frameAR = (float)m_textureWidth / textureVScale;
            float clientAreaAR = body.w / body.h;
            if (clientAreaAR > frameAR)
                body.w = body.h * frameAR;
            else
                body.h = body.w / frameAR;
        }
        else
        {
            float zoom = (float)(1 << (m_zoom - 1));
            body.w = m_textureWidth * zoom;
            body.h = textureVScale * zoom;
            body.x -= m_clientContentOffset.x;
            body.y -= m_clientContentOffset.y;
        }
        m_imageArea = body;
        m_viewRight = panelArea.x;
        m_viewBottom = infoArea.y;

        SDL_RenderTexture(renderer, m_texture, nullptr, &body);

        float pixelWidth = body.w / m_textureWidth;
        float pixelHeight = body.h / m_textureHeight;

        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

        // bad lines (from the current register values): raster $30-$f7 where the low 3 bits match YSCROLL,
        // with the display enabled. Shaded with a 20% overlay.
        {
            const u8* regs = m_raster.m_regs;
            if (regs[0x11] & 0x10)
            {
                int yscroll = regs[0x11] & 7;
                SDL_SetRenderDrawColor(renderer, 255, 0, 0, 51);
                for (int line = 0x30; line <= 0xf7 && line < m_textureHeight; line++)
                {
                    if ((line & 7) != yscroll)
                        continue;
                    SDL_FRect badLine{ body.x, body.y + line * pixelHeight, body.w, (std::max)(pixelHeight, 1.0f) };
                    SDL_RenderFillRect(renderer, &badLine);
                }
            }
        }

        // outline the 40x25 display window
        SDL_FRect displayArea{ body.x + VIC_DISPLAY_X * pixelWidth, body.y + VIC_DISPLAY_Y * pixelHeight, VIC_DISPLAY_W * pixelWidth, VIC_DISPLAY_H * pixelHeight };
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 64);
        SDL_RenderRect(renderer, &displayArea);

        // sprite boxes for all enabled sprites (from the current register values)
        {
            SDL_Rect oldClip{ 0,0,0,0 };
            bool hadClip = SDL_RenderClipEnabled(renderer);
            SDL_GetRenderClipRect(renderer, &oldClip);
            SDL_Rect imageClip{ (int)body.x, (int)body.y, (int)body.w, (int)body.h };
            SDL_Rect clip = imageClip;
            if (hadClip)
                SDL_GetRectIntersection(&oldClip, &imageClip, &clip);
            SDL_SetRenderClipRect(renderer, &clip);

            const u8* regs = m_raster.m_regs;
            for (int i = 0; i < 8; i++)
            {
                if (!(regs[0x15] & (1 << i)))
                    continue;

                int sx = regs[i * 2] | (((regs[0x10] >> i) & 1) << 8);
                int sy = regs[i * 2 + 1];
                int sw = (regs[0x1d] & (1 << i)) ? 48 : 24;
                int sh = (regs[0x17] & (1 << i)) ? 42 : 21;

                // sprite is displayed from the line after its Y compare matches; positions wrap around the raster
                int rx = (sx + VIC_SPRITE_X_OFFSET) % m_textureWidth;
                int ry = (sy + 1) % m_textureHeight;

                SDL_Color col = ToSDLColor(C64Palette[regs[0x27 + i] & 15]);
                for (int wy = 0; wy < 2; wy++)
                {
                    for (int wx = 0; wx < 2; wx++)
                    {
                        // draw the wrapped copies too, so a sprite crossing the raster edge shows on both sides
                        float x = (float)(rx - wx * m_textureWidth);
                        float y = (float)(ry - wy * m_textureHeight);
                        if (x + sw <= 0 || y + sh <= 0)
                            continue;

                        SDL_FRect sprArea{ body.x + x * pixelWidth, body.y + y * pixelHeight, sw * pixelWidth, sh * pixelHeight };
                        SDL_FRect shadow{ sprArea.x + 1, sprArea.y + 1, sprArea.w, sprArea.h };
                        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
                        SDL_RenderRect(renderer, &shadow);
                        SDL_SetRenderDrawColor(renderer, 255, 0, 255, 255);
                        SDL_RenderRect(renderer, &sprArea);
                        SDL_FRect swatch{ sprArea.x + 2, sprArea.y + 2, 6, 6 };
                        SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, 255);
                        SDL_RenderFillRect(renderer, &swatch);
                        fr.RenderText(renderer, std::format("{}", i), SDL_Color{ 255, 0, 255, 255 }, (int)sprArea.x + 10, (int)sprArea.y - 2, FontType::Text);
                    }
                }
            }

            SDL_SetRenderClipRect(renderer, hadClip ? &oldClip : nullptr);
        }

        // beam: highlight the current line and the last drawn cycle (8 pixels)
        float lineY = body.y + m_raster.m_line * pixelHeight;
        SDL_FRect lineArea{ body.x, lineY, body.w, (std::max)(pixelHeight, 1.0f) };
        SDL_SetRenderDrawColor(renderer, 255, 255, 0, 48);
        SDL_RenderFillRect(renderer, &lineArea);

        if (m_raster.m_liveCount > 0)
        {
            SDL_FRect cycleArea{ body.x + (m_raster.m_liveCount - 8) * pixelWidth, lineY - pixelHeight, pixelWidth * 8.0f, pixelHeight * 3.0f };
            SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
            SDL_RenderRect(renderer, &cycleArea);
        }

        // the raster point: red = break, green = refresh
        if (m_pointMode == VicRasterPointMode::Break || m_pointMode == VicRasterPointMode::Refresh)
        {
            SDL_FRect pointArea{ body.x + m_pointBlock * 8 * pixelWidth, body.y + (m_pointLine - 1) * pixelHeight, pixelWidth * 8.0f, pixelHeight * 3.0f };
            if (m_pointMode == VicRasterPointMode::Break)
                SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
            else
                SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
            SDL_RenderRect(renderer, &pointArea);
        }

        // mouse hover -> line/cycle readout
        m_hoverLine = -1;
        m_hoverPixel = -1;
        if (m_mouseX >= 0 && pixelWidth > 0.0f && pixelHeight > 0.0f)
        {
            int px = (int)((m_mouseX - body.x) / pixelWidth);
            int py = (int)((m_mouseY - body.y) / pixelHeight);
            if (px >= 0 && px < m_textureWidth && py >= 0 && py < m_textureHeight && m_mouseY >= m_clientArea.y + LINE_HEIGHT && m_mouseX < panelArea.x && m_mouseY < infoArea.y)
            {
                hoverX = px;
                hoverLine = py;
                m_hoverLine = py;
                m_hoverPixel = px;
                hoverCycle = (px / 8 + 1) % m_raster.m_cyclesPerLine;

                SDL_FRect hoverArea{ body.x + (px & ~7) * pixelWidth, body.y + py * pixelHeight, pixelWidth * 8.0f, (std::max)(pixelHeight, 1.0f) };
                SDL_SetRenderDrawColor(renderer, 0, 255, 255, 255);
                SDL_RenderRect(renderer, &hoverArea);
            }
        }

        if (m_zoom != 0)
        {
            m_clientContentSize.x = (int)(body.w + panelWidth);
            m_clientContentSize.y = (int)(body.h + infoHeight) + LINE_HEIGHT;
        }
        else
        {
            m_clientContentSize.x = 0;
            m_clientContentSize.y = 0;
        }
        LayoutScrollbars();
    }

    // info panel: where the VIC is reading its data from, and what mode it is in
    {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderFillRect(renderer, &infoArea);
        SDL_SetRenderDrawColor(renderer, 80, 80, 80, 255);
        SDL_RenderLine(renderer, infoArea.x, infoArea.y, infoArea.x + infoArea.w, infoArea.y);

        SDL_Color labelCol{ 160, 160, 160, 255 };
        SDL_Color valueCol{ 255, 255, 255, 255 };
        SDL_Color unusedCol{ 90, 90, 90, 255 };

        const u8* vr = m_raster.m_regs;
        int vmode = VicGraphicsMode(vr);
        bool bitmapMode = (vmode & 2) != 0;
        int screenAddr = m_raster.m_bank + (vr[0x18] >> 4) * 0x400;
        int bitmapAddr = m_raster.m_bank + ((vr[0x18] & 8) ? 0x2000 : 0);
        int charAddr = m_raster.m_bank + ((vr[0x18] >> 1) & 7) * 0x800;

        const char* colourMode = (vmode >= 5) ? "Invalid (ECM + MCM/BMM)" : (vmode & 4) ? "Extended colour" : (vmode & 1) ? "Multicolour" : "Hires";
        const char* displayMode = bitmapMode ? "Bitmap" : "Screen map (text)";

        int x0 = (int)infoArea.x + 8;
        int y0 = (int)infoArea.y + 4;

        // draws "label value" and returns the x position for the next field
        auto field = [&](int x, int row, const char* label, const std::string& value, const SDL_Color& col) -> int
            {
                int y = y0 + row * REG_ROW_HEIGHT;
                fr.RenderText(renderer, label, labelCol, x, y, FontType::Text);
                x += ((int)strlen(label) + 1) * charWidth;
                fr.RenderText(renderer, value, col, x, y, FontType::Text);
                return x + ((int)value.size() + 3) * charWidth;
            };

        int x = x0;
        x = field(x, 0, "Bank", std::format("${:04X}", m_raster.m_bank), valueCol);
        x = field(x, 0, "Screen", std::format("${:04X}", screenAddr), valueCol);
        x = field(x, 0, "Bitmap", std::format("${:04X}", bitmapAddr), bitmapMode ? valueCol : unusedCol);
        x = field(x, 0, "Charset", std::format("${:04X}", charAddr), bitmapMode ? unusedCol : valueCol);

        x = x0;
        x = field(x, 1, "Colour", colourMode, valueCol);
        x = field(x, 1, "Display", displayMode, valueCol);

        x = x0;
        x = field(x, 2, "Beam", std::format("line {:3d} (${:03X}) cycle {:2d}", m_raster.m_line, m_raster.m_line, m_raster.m_liveCount / 8), valueCol);
        if (hoverLine >= 0)
            x = field(x, 2, "Mouse", std::format("line {:3d} (${:03X}) cycle {:2d} x {:3d}", hoverLine, hoverLine, hoverCycle, hoverX), valueCol);
    }

    // register panel
    {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderFillRect(renderer, &panelArea);
        SDL_SetRenderDrawColor(renderer, 80, 80, 80, 255);
        SDL_RenderLine(renderer, panelArea.x, panelArea.y, panelArea.x, panelArea.y + panelArea.h);

        SDL_Color addrCol = tp.Col(ThemeColor::TextOperator);
        SDL_Color nameCol{ 160, 160, 160, 255 };
        SDL_Color valueCol{ 255, 255, 255, 255 };
        SDL_Color decodeCol = tp.Col(ThemeColor::TextString);

        const u8* w = m_raster.m_regs;       // as written
        const u8* r = m_raster.m_regsRead;   // as read by the CPU
        for (int reg = 0; reg < VIC_REG_COUNT; reg++)
        {
            int col = reg / panelRows;
            int row = reg % panelRows;
            int x = (int)panelArea.x + 8 + col * panelColWidth;
            int y = (int)panelArea.y + 4 + row * REG_ROW_HEIGHT;
            auto& info = s_vicRegs[reg];

            fr.RenderText(renderer, std::format("D0{:02X}", reg), addrCol, x, y, FontType::Text);
            fr.RenderText(renderer, info.m_name, nameCol, x + 5 * charWidth, y, FontType::Text);
            fr.RenderText(renderer, std::format("{:02X}", r[reg]), valueCol, x + 12 * charWidth, y, FontType::Text);

            int dx = x + 15 * charWidth;
            std::string decode;
            switch (info.m_decode)
            {
                case VicRegDecode::SpriteX:
                    decode = std::format("{:3d}", w[reg] | (((w[0x10] >> (reg / 2)) & 1) << 8));
                    break;
                case VicRegDecode::SpriteY:
                    decode = std::format("{:3d}", w[reg]);
                    break;
                case VicRegDecode::Bits:
                    decode = std::format("{:08b}", r[reg]);
                    break;
                case VicRegDecode::RasterRead:
                    decode = std::format("irq {:3d}", ((w[0x11] & 0x80) << 1) | w[0x12]);
                    break;
                case VicRegDecode::Colour:
                {
                    SDL_Color sc = ToSDLColor(C64Palette[r[reg] & 15]);
                    SDL_FRect swatch{ (float)dx, (float)y + 3, (float)charWidth * 2, (float)REG_ROW_HEIGHT - 6 };
                    SDL_SetRenderDrawColor(renderer, sc.r, sc.g, sc.b, 255);
                    SDL_RenderFillRect(renderer, &swatch);
                    SDL_SetRenderDrawColor(renderer, 80, 80, 80, 255);
                    SDL_RenderRect(renderer, &swatch);
                    decode = std::format("   {:2d}", r[reg] & 15);
                }
                break;
                default:
                    break;
            }
            if (!decode.empty())
                fr.RenderText(renderer, decode, decodeCol, dx, y, FontType::Text);
        }
    }

    // header + icons
    auto& ir = IconRenderer::Instance();
    SDL_FRect headerArea = m_clientArea.AsSDLFRect();
    headerArea.h = LINE_HEIGHT;
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderFillRect(renderer, &headerArea);
    ir.DrawIcon(renderer, Icons::Run, m_clientArea.x + 16, m_clientArea.y + 12);
    ir.DrawIcon(renderer, Icons::Pause, m_clientArea.x + 36, m_clientArea.y + 12);
    ir.DrawIcon(renderer, Icons::SingleStep, m_clientArea.x + 56, m_clientArea.y + 12);
    ir.DrawIcon(renderer, Icons::Zoom, m_clientArea.x + 76, m_clientArea.y + 12);

    if (highlight.m_highlight == WindowHighlightType::EmuScreenIcon && highlight.m_window == this)
        ir.DrawIcon(renderer, Icons::Highlight, highlight.m_area.x + 8, highlight.m_area.y + 8);

    // status line: same emulator state as the EmuScreen
    SDL_Color stateCol{ 255,255,255,255 };
    ViceState viceState = ViceBridge::Instance().GetViceState();
    std::string text = std::format("PC {:4x} ROW {:03x} COL {:3d} A {:2x} X {:2x} Y {:2x} Flags {}{}.{}{}{}{}{} Zoom {} Mode {}",
        viceState.m_pc, viceState.m_rasterLine, viceState.m_rasterCycle, viceState.m_acc, viceState.m_x, viceState.m_y,
        viceState.m_flags & 128 ? 'N' : 'n',
        viceState.m_flags & 64 ? 'V' : 'v',
        viceState.m_flags & 16 ? 'B' : 'b',
        viceState.m_flags & 8 ? 'D' : 'd',
        viceState.m_flags & 4 ? 'I' : 'i',
        viceState.m_flags & 2 ? 'Z' : 'z',
        viceState.m_flags & 1 ? 'C' : 'c',
        s_zoomText[m_zoom],
        isPal ? "PAL" : "NTSC");
    fr.RenderText(renderer, text, stateCol, m_clientArea.x + 100, m_clientArea.y, FontType::UI);
}

// Convert a mouse position to a raster line / pixel. Returns false if it is not over the visible image.
bool VicScreenWindow::MouseToRaster(float mouseX, float mouseY, int& line, int& pixel)
{
    if (m_textureWidth <= 0 || m_textureHeight <= 0 || m_imageArea.w <= 0.0f || m_imageArea.h <= 0.0f)
        return false;
    if (mouseX >= m_viewRight || mouseY >= m_viewBottom || mouseY < m_clientArea.y + LINE_HEIGHT || mouseX < m_clientArea.x)
        return false;

    int px = (int)floorf((mouseX - m_imageArea.x) * m_textureWidth / m_imageArea.w);
    int py = (int)floorf((mouseY - m_imageArea.y) * m_textureHeight / m_imageArea.h);
    if (px < 0 || px >= m_textureWidth || py < 0 || py >= m_textureHeight)
        return false;

    line = py;
    pixel = px;
    return true;
}

// Place the raster point in the given mode. Returns true if the point is now placed.
// With allowToggle, placing it where it already is (in the same mode) removes it instead.
bool VicScreenWindow::PlaceRasterPoint(VicRasterPointMode mode, int line, int block, bool allowToggle)
{
    auto& vb = ViceBridge::Instance();

    if (m_pointMode == mode && m_pointLine == line && m_pointBlock == block)
    {
        if (!allowToggle)
            return true;

        // toggle off: back to refreshing at the top of the frame
        m_pointMode = VicRasterPointMode::TopOfFrame;
        vb.SetRasterPoint(VicRasterPointMode::TopOfFrame);
        return false;
    }

    m_pointMode = mode;
    m_pointLine = line;
    m_pointBlock = block;

    // the beam has output an 8 pixel block once its cycle count reaches block + 1
    vb.SetRasterPoint(mode, m_pointLine, m_pointBlock + 1);

    // a point placed or moved while stopped resumes the emulator: red runs to the point, green just keeps running
    if (vb.HasViceStopped())
        vb.Continue();
    return true;
}

bool VicScreenWindow::HandleEvent(SDL_Event* e)
{
    switch (e->type)
    {
        case SDL_EVENT_MOUSE_BUTTON_UP:
            m_dragMode = VicRasterPointMode::Off;
            break;

        case SDL_EVENT_MOUSE_MOTION:
        {
            // drag the raster point while the button that placed it is held
            if (m_dragMode == VicRasterPointMode::Off)
                break;

            u32 mask = (m_dragMode == VicRasterPointMode::Break) ? SDL_BUTTON_LMASK : SDL_BUTTON_RMASK;
            if (!(e->motion.state & mask))
            {
                m_dragMode = VicRasterPointMode::Off;
                break;
            }

            int line, pixel;
            if (MouseToRaster(e->motion.x, e->motion.y, line, pixel))
                PlaceRasterPoint(m_dragMode, line, pixel / 8, false);
            return true;
        }

        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        {
            auto& selected = WindowManager::Instance().GetWindowSelectionQuery();
            m_dragMode = VicRasterPointMode::Off;

            // right click on the image: refresh point (green)
            if (e->button.button == SDL_BUTTON_RIGHT)
            {
                auto& hover = WindowManager::Instance().GetWindowHighlightQuery();
                int line, pixel;
                if (hover.m_window == this && hover.m_highlight == WindowHighlightType::ClientArea && MouseToRaster(e->button.x, e->button.y, line, pixel))
                {
                    if (PlaceRasterPoint(VicRasterPointMode::Refresh, line, pixel / 8, true))
                        m_dragMode = VicRasterPointMode::Refresh;
                    return true;
                }
                return false;
            }

            // left click on the image: raster breakpoint (red)
            int line, pixel;
            if (e->button.button == SDL_BUTTON_LEFT && selected.m_highlight == WindowHighlightType::ClientArea && selected.m_window == this && MouseToRaster(e->button.x, e->button.y, line, pixel))
            {
                if (PlaceRasterPoint(VicRasterPointMode::Break, line, pixel / 8, true))
                    m_dragMode = VicRasterPointMode::Break;
                return true;
            }
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
                        if (SDL_GetModState() & SDL_KMOD_SHIFT)
                            ViceBridge::Instance().MultiStep();
                        else
                            ViceBridge::Instance().SingleStep();
                        break;

                    case Icons::Zoom:
                        m_zoom = (m_zoom + 1) % 4;
                        m_clientContentOffset = { 0, 0 };
                        break;

                    default:
                        break;
                }
                return true;
            }
        }
        break;
    }
    return false;
}

void VicScreenWindow::SaveTokens(std::vector<std::string>& layoutTokens)
{
    layoutTokens.push_back("VICSCREEN");
}

bool VicScreenWindow::CreateFromLayoutTokens(WindowLayout* layout, const std::vector<std::string>& layoutTokens, size_t& idx)
{
    if (layoutTokens[idx] != "VICSCREEN")
        return false;

    idx++;
    layout->m_tabs.push_back(new VicScreenWindow());
    return true;
}
