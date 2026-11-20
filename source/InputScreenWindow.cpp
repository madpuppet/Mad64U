#include "common.h"
#include "InputScreenWindow.h"
#include "WindowLayout.h"
#include "ViceBridge.h"
#include "WindowManager.h"
#include "Application.h"

#define INPUT_MARGIN 8
#define INPUT_SECTION_GAP 24
#define INPUT_ROW_HEIGHT 18
#define MATRIX_CELL 16
#define JOY_CELL 22
#define PADDLE_BAR_WIDTH 128

static const SDL_Color s_labelCol{ 160, 160, 160, 255 };
static const SDL_Color s_valueCol{ 255, 255, 255, 255 };
static const SDL_Color s_titleCol{ 240, 220, 100, 255 };
static const SDL_Color s_offCol{ 90, 90, 90, 255 };

// held keys / pressed directions
static const SDL_Color s_onFill{ 255, 200, 60, 255 };
static const SDL_Color s_offFill{ 55, 55, 55, 255 };
static const SDL_Color s_edge{ 110, 110, 110, 255 };

//==================================================================================================================================
// C64 keyboard layout
//
// Positions and sizes are in key units. row / col are the key's place in the CIA1 matrix (port A bit / port B bit);
// RESTORE and SHIFT LOCK are not in the matrix (RESTORE drives NMI, SHIFT LOCK latches the left shift line) and show as -1.
//==================================================================================================================================

struct C64Key
{
    const char* m_label;
    const char* m_short;        // used when the label doesn't fit the key
    float m_x, m_y, m_w;
    int m_row, m_col;
};

static const C64Key s_keys[] =
{
    // row 0
    { "<-", "<-", 0, 0, 1, 7, 1 },  { "1", "1", 1, 0, 1, 7, 0 },   { "2", "2", 2, 0, 1, 7, 3 },   { "3", "3", 3, 0, 1, 1, 0 },
    { "4", "4", 4, 0, 1, 1, 3 },    { "5", "5", 5, 0, 1, 2, 0 },   { "6", "6", 6, 0, 1, 2, 3 },   { "7", "7", 7, 0, 1, 3, 0 },
    { "8", "8", 8, 0, 1, 3, 3 },    { "9", "9", 9, 0, 1, 4, 0 },   { "0", "0", 10, 0, 1, 4, 3 },  { "+", "+", 11, 0, 1, 5, 0 },
    { "-", "-", 12, 0, 1, 5, 3 },   { "\xC2\xA3", "\xC2\xA3", 13, 0, 1, 6, 0 },                   { "HOME", "HM", 14, 0, 1, 6, 3 },
    { "DEL", "DL", 15, 0, 1, 0, 0 },
    { "F1", "F1", 16.75f, 0, 1.5f, 0, 4 },

    // row 1
    { "CTRL", "CTL", 0, 1, 1.5f, 7, 2 },
    { "Q", "Q", 1.5f, 1, 1, 7, 6 },  { "W", "W", 2.5f, 1, 1, 1, 1 },  { "E", "E", 3.5f, 1, 1, 1, 6 },  { "R", "R", 4.5f, 1, 1, 2, 1 },
    { "T", "T", 5.5f, 1, 1, 2, 6 },  { "Y", "Y", 6.5f, 1, 1, 3, 1 },  { "U", "U", 7.5f, 1, 1, 3, 6 },  { "I", "I", 8.5f, 1, 1, 4, 1 },
    { "O", "O", 9.5f, 1, 1, 4, 6 },  { "P", "P", 10.5f, 1, 1, 5, 1 }, { "@", "@", 11.5f, 1, 1, 5, 6 }, { "*", "*", 12.5f, 1, 1, 6, 1 },
    { "^", "^", 13.5f, 1, 1, 6, 6 },
    { "RESTORE", "RST", 14.5f, 1, 1.5f, -1, -1 },
    { "F3", "F3", 16.75f, 1, 1.5f, 0, 5 },

    // row 2
    { "STOP", "STP", 0, 2, 1, 7, 7 },
    { "LOCK", "LCK", 1, 2, 0.75f, -1, -1 },
    { "A", "A", 1.75f, 2, 1, 1, 2 }, { "S", "S", 2.75f, 2, 1, 1, 5 }, { "D", "D", 3.75f, 2, 1, 2, 2 }, { "F", "F", 4.75f, 2, 1, 2, 5 },
    { "G", "G", 5.75f, 2, 1, 3, 2 }, { "H", "H", 6.75f, 2, 1, 3, 5 }, { "J", "J", 7.75f, 2, 1, 4, 2 }, { "K", "K", 8.75f, 2, 1, 4, 5 },
    { "L", "L", 9.75f, 2, 1, 5, 2 }, { ":", ":", 10.75f, 2, 1, 5, 5 }, { ";", ";", 11.75f, 2, 1, 6, 2 }, { "=", "=", 12.75f, 2, 1, 6, 5 },
    { "RETURN", "RET", 13.75f, 2, 2.25f, 0, 1 },
    { "F5", "F5", 16.75f, 2, 1.5f, 0, 6 },

    // row 3
    { "C=", "C=", 0, 3, 1, 7, 5 },
    { "SHIFT", "SHF", 1, 3, 1.25f, 1, 7 },
    { "Z", "Z", 2.25f, 3, 1, 1, 4 }, { "X", "X", 3.25f, 3, 1, 2, 7 }, { "C", "C", 4.25f, 3, 1, 2, 4 }, { "V", "V", 5.25f, 3, 1, 3, 7 },
    { "B", "B", 6.25f, 3, 1, 3, 4 }, { "N", "N", 7.25f, 3, 1, 4, 7 }, { "M", "M", 8.25f, 3, 1, 4, 4 }, { ",", ",", 9.25f, 3, 1, 5, 7 },
    { ".", ".", 10.25f, 3, 1, 5, 4 }, { "/", "/", 11.25f, 3, 1, 6, 7 },
    { "SHIFT", "SHF", 12.25f, 3, 1.75f, 6, 4 },
    { "v", "v", 14, 3, 1, 0, 7 },   // CRSR down
    { ">", ">", 15, 3, 1, 0, 2 },   // CRSR right
    { "F7", "F7", 16.75f, 3, 1.5f, 0, 3 },

    // row 4
    { "SPACE", "SPC", 3.25f, 4, 9, 7, 4 },
};

#define KEYBOARD_UNITS_W 18.25f
#define KEYBOARD_UNITS_H 5.0f
#define KEY_HEIGHT_SCALE 1.25f      // keys are taller than wide, to fit the label above the SHIFT / C= characters

// PETSCII -> screen code (the char ROM index) for printable characters; false for control codes
static bool PetsciiToScreen(u8 p, int& screen)
{
    if (p >= 0x20 && p <= 0x3f) screen = p;
    else if (p >= 0x40 && p <= 0x5f) screen = p - 0x40;
    else if (p >= 0x60 && p <= 0x7f) screen = p - 0x20;
    else if (p >= 0xa0 && p <= 0xbf) screen = p - 0x40;
    else if (p >= 0xc0 && p <= 0xdf) screen = p - 0x80;
    else if (p >= 0xe0 && p <= 0xfe) screen = p - 0x80;
    else if (p == 0xff) screen = 0x5e;
    else return false;
    return true;
}

// PETSCII function key codes -> function key number (1-8), 0 if not a function key
static int PetsciiToFunctionKey(u8 p)
{
    static const int fkeys[8] = { 1, 3, 5, 7, 2, 4, 6, 8 };    // $85-$8c
    return (p >= 0x85 && p <= 0x8c) ? fkeys[p - 0x85] : 0;
}

// width of a key code as drawn by DrawKeyCode, in characters
static int KeyCodeChars(u8 p)
{
    return PetsciiToFunctionKey(p) ? 2 : 1;
}

// PETSCII colour control codes -> C64 colour index, -1 if not a colour code
static int PetsciiToColour(u8 p)
{
    switch (p)
    {
        case 0x90: return 0;    // black
        case 0x05: return 1;    // white
        case 0x1c: return 2;    // red
        case 0x9f: return 3;    // cyan
        case 0x9c: return 4;    // purple
        case 0x1e: return 5;    // green
        case 0x1f: return 6;    // blue
        case 0x9e: return 7;    // yellow
        case 0x81: return 8;    // orange
        case 0x95: return 9;    // brown
        case 0x96: return 10;   // light red
        case 0x97: return 11;   // dark grey
        case 0x98: return 12;   // grey
        case 0x99: return 13;   // light green
        case 0x9a: return 14;   // light blue
        case 0x9b: return 15;   // light grey
    }
    return -1;
}

// names of the key at each matrix position, [row][col]
static const char* s_matrixNames[8][8] =
{
    { "DEL", "RETURN", "CRSR RIGHT", "F7", "F1", "F3", "F5", "CRSR DOWN" },
    { "3", "W", "A", "4", "Z", "S", "E", "LEFT SHIFT" },
    { "5", "R", "D", "6", "C", "F", "T", "X" },
    { "7", "Y", "G", "8", "B", "H", "U", "V" },
    { "9", "I", "J", "0", "M", "K", "O", "N" },
    { "+", "P", "L", "-", ".", ":", "@", "," },
    { "\xC2\xA3", "*", ";", "HOME", "RIGHT SHIFT", "=", "^", "/" },
    { "1", "<-", "CTRL", "2", "SPACE", "C=", "Q", "RUN/STOP" },
};

static void FillRect(SDL_Renderer* renderer, const SDL_FRect& r, const SDL_Color& c)
{
    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
    SDL_RenderFillRect(renderer, &r);
}

static void OutlineRect(SDL_Renderer* renderer, const SDL_FRect& r, const SDL_Color& c)
{
    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
    SDL_RenderRect(renderer, &r);
}

// draws text centred in a rectangle; returns false (and draws nothing) if it doesn't fit the width
static bool TextInRect(SDL_Renderer* renderer, const char* text, const SDL_Color& col, const SDL_FRect& r, bool force)
{
    auto& fr = FontRenderer::Instance();
    Recti area;
    fr.CalcTextArea(renderer, text, { 0, 0 }, FontType::Text, area);
    if (!force && area.w > r.w - 4)
        return false;
    fr.RenderText(renderer, text, col, (int)(r.x + (r.w - area.w) * 0.5f), (int)(r.y + (r.h - area.h) * 0.5f), FontType::Text);
    return true;
}

InputScreenWindow::InputScreenWindow()
{
    m_name = "InputScreen";
}

InputScreenWindow::~InputScreenWindow()
{
    if (m_glyphTexture)
        SDL_DestroyTexture(m_glyphTexture);
}

// (re)build the glyph texture when the renderer or the character ROM changes
void InputScreenWindow::UpdateGlyphTexture(SDL_Renderer* renderer)
{
    if (m_glyphTexture && renderer == m_glyphRenderer && memcmp(m_glyphRom, m_charRom, sizeof(m_glyphRom)) == 0)
        return;

    if (m_glyphTexture)
        SDL_DestroyTexture(m_glyphTexture);
    m_glyphRenderer = renderer;
    memcpy(m_glyphRom, m_charRom, sizeof(m_glyphRom));

    // 16 x 8 characters of 8x8 pixels, bit 7 of each byte is the leftmost pixel
    static u32 pixels[128 * 64];
    for (int ch = 0; ch < 128; ch++)
    {
        int cx = (ch & 15) * 8;
        int cy = (ch >> 4) * 8;
        for (int y = 0; y < 8; y++)
        {
            u8 bits = m_glyphRom[ch * 8 + y];
            for (int x = 0; x < 8; x++)
                pixels[(cy + y) * 128 + cx + x] = (bits & (0x80 >> x)) ? 0xffffffff : 0x00000000;
        }
    }

    m_glyphTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, 128, 64);
    if (!m_glyphTexture)
        return;
    SDL_UpdateTexture(m_glyphTexture, nullptr, pixels, 128 * 4);
    SDL_SetTextureBlendMode(m_glyphTexture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(m_glyphTexture, SDL_SCALEMODE_NEAREST);
}

// what a key types with a modifier: a printable character as its C64 glyph, a function key as "Fn" (KeyCodeChars
// wide), a colour code as a colour swatch, anything else (cursor / control codes) as nothing
void InputScreenWindow::DrawKeyCode(SDL_Renderer* renderer, u8 petscii, float x, float y, float size, const SDL_Color& col)
{
    auto glyph = [&](int screen, float gx)
        {
            if (!m_glyphTexture)
                return;
            SDL_FRect src{ (float)((screen & 15) * 8), (float)((screen >> 4) * 8), 8.0f, 8.0f };
            SDL_FRect dst{ gx, y, size, size };
            SDL_SetTextureColorMod(m_glyphTexture, col.r, col.g, col.b);
            SDL_RenderTexture(renderer, m_glyphTexture, &src, &dst);
        };

    int screen;
    if (PetsciiToScreen(petscii, screen))
    {
        glyph(screen, x);
        return;
    }

    int fkey = PetsciiToFunctionKey(petscii);
    if (fkey)
    {
        glyph(0x06, x);                 // 'F'
        glyph(0x30 + fkey, x + size);   // digit
        return;
    }

    int colour = PetsciiToColour(petscii);
    if (colour >= 0)
    {
        u32 argb = C64Palette[colour];
        SDL_FRect swatch{ x + 1, y + 1, size - 2, size - 2 };
        FillRect(renderer, swatch, SDL_Color{ (u8)(argb >> 16), (u8)(argb >> 8), (u8)argb, 255 });
        OutlineRect(renderer, swatch, col);
    }
}

void InputScreenWindow::MessageChild(WindowLayout* layout, struct WindowMessageStruct& msg)
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

bool InputScreenWindow::Tick()
{
    return NeedsRefresh();
}

bool InputScreenWindow::HandleEvent(SDL_Event* e)
{
    if (e->type == SDL_EVENT_MOUSE_BUTTON_DOWN && (e->button.button == SDL_BUTTON_LEFT || e->button.button == SDL_BUTTON_RIGHT))
    {
        SDL_FPoint p{ e->button.x, e->button.y };
        if (ViceBridge::Exists() && SDL_PointInRectFloat(&p, &m_modeButton))
        {
            int count = (int)ControllerMode::Count;
            int step = (e->button.button == SDL_BUTTON_LEFT) ? 1 : count - 1;
            int mode = ((int)ViceBridge::Instance().GetControllerMode() + step) % count;
            Application::Instance().Vice_SetControllerMode((ControllerMode)mode);
            return true;
        }
    }
    return false;
}

// what each controller mode does, for the line next to the mode button
static const char* ControllerModeHelp(ControllerMode mode)
{
    switch (mode)
    {
        case ControllerMode::DualStick:   return "pad 1: left stick + A fire + B up -> joy 1,  right stick + X fire + Y up -> joy 2";
        case ControllerMode::DualPaddles: return "pad 1: left stick -> paddles 1 (A / B fire),  right stick -> paddles 2 (X / Y fire)";
        case ControllerMode::Joy1:        return "pad 1 -> joy 1, pad 2 -> joy 2  (stick / d-pad, A fire, B up)";
        case ControllerMode::Joy2:        return "pad 1 -> joy 2, pad 2 -> joy 1  (stick / d-pad, A fire, B up)";
        case ControllerMode::Paddle1:     return "pad 1 -> paddles 1, pad 2 -> paddles 2  (left stick POTX / POTY, A / B fire)";
        case ControllerMode::Paddle2:     return "pad 1 -> paddles 2, pad 2 -> paddles 1  (left stick POTX / POTY, A / B fire)";
        case ControllerMode::MousePad1:   return "mouse -> paddles 1 (X / Y -> POTX / POTY, left / right button fire), pad 1 -> joy 2.  Click the emu screen to capture";
        case ControllerMode::MousePad2:   return "mouse -> paddles 2 (X / Y -> POTX / POTY, left / right button fire), pad 1 -> joy 1.  Click the emu screen to capture";
        default:                          return "";
    }
}

bool InputScreenWindow::NeedsRefresh()
{
    if (!ViceBridge::Exists())
        return false;
    auto& vb = ViceBridge::Instance();
    return vb.GetInputStateVersion() != m_stateVersion || vb.GetControllerMode() != m_shownMode;
}

static bool KeyHeld(const InputState& state, int row, int col)
{
    return row >= 0 && (state.m_keyMatrix[row] & (1 << col)) != 0;
}

void InputScreenWindow::DrawKeyboard(SDL_Renderer* renderer, float x, float y, float unit)
{
    float keyH = unit * KEY_HEIGHT_SCALE;

    // the C= and SHIFT characters along the bottom of each key, like the key fronts: C= on the left, SHIFT on the right.
    // Holding a modifier highlights the characters it selects.
    float glyph = unit >= 36.0f ? 16.0f : 8.0f;
    bool shiftHeld = KeyHeld(m_state, 1, 7) || KeyHeld(m_state, 6, 4);
    bool cbmHeld = KeyHeld(m_state, 7, 5);
    SDL_Color glyphCol{ 200, 200, 200, 255 };
    SDL_Color activeCol{ 255, 220, 80, 255 };
    SDL_Color dimCol{ 100, 100, 100, 255 };

    for (auto& key : s_keys)
    {
        SDL_FRect r{ x + key.m_x * unit + 1, y + key.m_y * keyH + 1, key.m_w * unit - 2, keyH - 2 };
        bool held = KeyHeld(m_state, key.m_row, key.m_col);
        bool inMatrix = key.m_row >= 0;

        FillRect(renderer, r, held ? s_onFill : inMatrix ? s_offFill : SDL_Color{ 35, 35, 35, 255 });
        OutlineRect(renderer, r, s_edge);

        // label in the top part of the key
        SDL_FRect labelArea{ r.x, r.y, r.w, r.h - glyph - 2 };
        SDL_Color textCol = held ? SDL_Color{ 0, 0, 0, 255 } : inMatrix ? SDL_Color{ 220, 220, 220, 255 } : s_offCol;
        if (!TextInRect(renderer, key.m_label, textCol, labelArea, false))
            TextInRect(renderer, key.m_short, textCol, labelArea, true);

        if (inMatrix && m_haveKeyTables)
        {
            int idx = key.m_row * 8 + key.m_col;
            float gy = r.y + r.h - glyph - 3;
            auto colFor = [&](bool active, bool otherActive)
                {
                    if (held)
                        return SDL_Color{ 0, 0, 0, 255 };
                    return active ? activeCol : otherActive ? dimCol : glyphCol;
                };
            u8 cbmCode = m_keyTables[2][idx];
            u8 shiftCode = m_keyTables[1][idx];

            // C= and SHIFT both give F2 / F4 / F6 / F8 on the function keys: show that once, as SHIFT
            if (cbmCode != shiftCode || !PetsciiToFunctionKey(shiftCode))
                DrawKeyCode(renderer, cbmCode, r.x + 3, gy, glyph, colFor(cbmHeld, shiftHeld));
            DrawKeyCode(renderer, shiftCode, r.x + r.w - glyph * KeyCodeChars(shiftCode) - 3, gy, glyph, colFor(shiftHeld, cbmHeld));
        }
    }
}

// the raw CIA1 keyboard matrix: rows are port A bits (driven by the CPU), columns port B bits (read back)
float InputScreenWindow::DrawMatrix(SDL_Renderer* renderer, float x, float y)
{
    auto& fr = FontRenderer::Instance();
    int charWidth = fr.GetCharactorWidth(FontType::Text, '0');
    float labelW = (float)(4 * charWidth);

    fr.RenderText(renderer, "Key matrix", s_titleCol, (int)x, (int)y, FontType::Text);
    y += INPUT_ROW_HEIGHT;
    fr.RenderText(renderer, "PB", s_labelCol, (int)x, (int)y, FontType::Text);
    for (int c = 0; c < 8; c++)
        TextInRect(renderer, std::format("{}", c).c_str(), s_labelCol, SDL_FRect{ x + labelW + c * MATRIX_CELL, y, MATRIX_CELL, INPUT_ROW_HEIGHT }, true);
    y += INPUT_ROW_HEIGHT;

    std::string held;
    for (int r = 0; r < 8; r++)
    {
        fr.RenderText(renderer, std::format("PA{}", r), s_labelCol, (int)x, (int)(y + r * MATRIX_CELL - 1), FontType::Text);
        for (int c = 0; c < 8; c++)
        {
            SDL_FRect cell{ x + labelW + c * MATRIX_CELL + 1, y + r * MATRIX_CELL + 1, MATRIX_CELL - 2, MATRIX_CELL - 2 };
            bool on = KeyHeld(m_state, r, c);
            FillRect(renderer, cell, on ? s_onFill : s_offFill);
            if (on)
            {
                if (!held.empty())
                    held += " ";
                held += s_matrixNames[r][c];
            }
        }
    }
    y += 8 * MATRIX_CELL + 4;

    fr.RenderText(renderer, held.empty() ? std::string("no keys held") : held, held.empty() ? s_offCol : s_valueCol, (int)x, (int)y, FontType::Text);
    return labelW + 8 * MATRIX_CELL;
}

float InputScreenWindow::DrawJoystick(SDL_Renderer* renderer, int port, float x, float y)
{
    auto& fr = FontRenderer::Instance();
    u8 v = m_state.m_joystick[port];

    fr.RenderText(renderer, std::format("Joystick {}", port + 1), s_titleCol, (int)x, (int)y, FontType::Text);
    y += INPUT_ROW_HEIGHT;
    const char* device = m_state.m_device[port][0] ? m_state.m_device[port] : "None";
    fr.RenderText(renderer, device, s_labelCol, (int)x, (int)y, FontType::Text);
    y += INPUT_ROW_HEIGHT + 4;

    // direction pad with fire in the middle
    struct Cell { int cx, cy, bit; const char* label; };
    static const Cell cells[] = { { 1, 0, 0, "U" }, { 1, 2, 1, "D" }, { 0, 1, 2, "L" }, { 2, 1, 3, "R" }, { 1, 1, 4, "F" } };
    for (auto& c : cells)
    {
        SDL_FRect r{ x + c.cx * JOY_CELL + 1, y + c.cy * JOY_CELL + 1, JOY_CELL - 2, JOY_CELL - 2 };
        bool on = (v & (1 << c.bit)) != 0;
        SDL_Color fill = on ? (c.bit == 4 ? SDL_Color{ 255, 70, 70, 255 } : s_onFill) : s_offFill;
        FillRect(renderer, r, fill);
        OutlineRect(renderer, r, s_edge);
        TextInRect(renderer, c.label, on ? SDL_Color{ 0, 0, 0, 255 } : s_labelCol, r, true);
    }

    fr.RenderText(renderer, std::format("${:02X}", v), v ? s_valueCol : s_offCol, (int)(x + 3 * JOY_CELL + 8), (int)(y + JOY_CELL), FontType::Text);

    Recti area;
    fr.CalcTextArea(renderer, device, { 0, 0 }, FontType::Text, area);
    return (std::max)((float)area.w, (float)(3 * JOY_CELL + 8 + 4 * fr.GetCharactorWidth(FontType::Text, '0')));
}

// POTX / POTY of each port. The SID only reads one port at a time (selected by CIA1 port A bits 6-7);
// paddle fire buttons are the joystick left (paddle X) and right (paddle Y) lines.
float InputScreenWindow::DrawPaddles(SDL_Renderer* renderer, float x, float y)
{
    auto& fr = FontRenderer::Instance();
    int charWidth = fr.GetCharactorWidth(FontType::Text, '0');

    fr.RenderText(renderer, "Paddles", s_titleCol, (int)x, (int)y, FontType::Text);
    y += INPUT_ROW_HEIGHT;

    float labelW = (float)(11 * charWidth);
    for (int port = 0; port < 2; port++)
    {
        bool selected = (m_state.m_potPortMask & (1 << port)) != 0;
        fr.RenderText(renderer, std::format("Port {}{}", port + 1, selected ? "  (read by SID)" : ""), selected ? s_valueCol : s_labelCol, (int)x, (int)y, FontType::Text);
        y += INPUT_ROW_HEIGHT;

        for (int axis = 0; axis < 2; axis++)
        {
            u8 pot = axis ? m_state.m_potY[port] : m_state.m_potX[port];
            bool fire = (m_state.m_joystick[port] & (axis ? 0x08 : 0x04)) != 0;

            fr.RenderText(renderer, std::format("{} ${:02X}", axis ? "POTY" : "POTX", pot), selected ? s_valueCol : s_labelCol, (int)x + 8, (int)y, FontType::Text);

            SDL_FRect bar{ x + 8 + labelW, y + 3, PADDLE_BAR_WIDTH, INPUT_ROW_HEIGHT - 6 };
            FillRect(renderer, bar, s_offFill);
            SDL_FRect level{ bar.x, bar.y, PADDLE_BAR_WIDTH * pot / 255.0f, bar.h };
            FillRect(renderer, level, selected ? SDL_Color{ 110, 200, 255, 255 } : SDL_Color{ 70, 110, 140, 255 });
            OutlineRect(renderer, bar, s_edge);

            SDL_FRect fireBox{ bar.x + bar.w + 8, bar.y, bar.h, bar.h };
            FillRect(renderer, fireBox, fire ? SDL_Color{ 255, 70, 70, 255 } : s_offFill);
            OutlineRect(renderer, fireBox, s_edge);
            fr.RenderText(renderer, "fire", fire ? s_valueCol : s_offCol, (int)(fireBox.x + fireBox.w + 4), (int)y, FontType::Text);
            y += INPUT_ROW_HEIGHT;
        }
    }
    return 8 + labelW + PADDLE_BAR_WIDTH + 8 + INPUT_ROW_HEIGHT + 6 * charWidth;
}

void InputScreenWindow::Paint(SDL_Renderer* renderer, const Recti& dirtyArea)
{
    if (ViceBridge::Exists())
    {
        auto& vb = ViceBridge::Instance();
        vb.GetInputState(m_state, m_stateVersion);
        m_haveKeyTables = vb.GetKeyboardTables(&m_keyTables[0][0], m_charRom);
        UpdateGlyphTexture(renderer);
    }

    auto& wm = WindowManager::Instance();
    auto& tp = Application::Instance().GetThemeProperties();
    if (wm.GetActiveWindowBase() == this)
        tp.SetRenderDrawColor(renderer, ThemeColor::SourceBackgroundSelected);
    else
        tp.SetRenderDrawColor(renderer, ThemeColor::SourceBackground);
    SDL_FRect body = m_clientArea.AsSDLFRect();
    SDL_RenderFillRect(renderer, &body);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    float x0 = (float)(m_clientArea.x - m_clientContentOffset.x + INPUT_MARGIN);
    float y0 = (float)(m_clientArea.y - m_clientContentOffset.y + INPUT_MARGIN);
    auto& fr = FontRenderer::Instance();

    // controller mode button
    float y = y0;
    if (ViceBridge::Exists())
    {
        auto& vb = ViceBridge::Instance();
        ControllerMode mode = vb.GetControllerMode();
        std::string text = std::format("Controller: {}", ControllerModeName(mode));
        Recti area;
        fr.CalcTextArea(renderer, text, { 0, 0 }, FontType::Text, area);
        m_modeButton = SDL_FRect{ x0, y, (float)area.w + 16, (float)INPUT_ROW_HEIGHT + 6 };

        m_shownMode = mode;
        FillRect(renderer, m_modeButton, SDL_Color{ 50, 50, 70, 255 });
        OutlineRect(renderer, m_modeButton, SDL_Color{ 140, 140, 200, 255 });
        TextInRect(renderer, text.c_str(), s_valueCol, m_modeButton, true);

        std::string help = ControllerModeHelp(mode);
        if (vb.GetControllerCount() == 0)
            help += "   (no controller connected)";
        fr.RenderText(renderer, help, s_labelCol, (int)(m_modeButton.x + m_modeButton.w + 12), (int)(y + 3), FontType::Text);
        y += m_modeButton.h + 10;
    }

    // keyboard, scaled to the window width
    float unit = std::clamp((m_clientArea.w - 2.0f * INPUT_MARGIN) / KEYBOARD_UNITS_W, 26.0f, 44.0f);
    DrawKeyboard(renderer, x0, y, unit);
    float keyboardW = KEYBOARD_UNITS_W * unit;
    y += KEYBOARD_UNITS_H * unit * KEY_HEIGHT_SCALE + INPUT_SECTION_GAP;

    // matrix, joysticks and paddles side by side underneath
    float x = x0;
    x += DrawMatrix(renderer, x, y) + INPUT_SECTION_GAP;
    x += DrawJoystick(renderer, 0, x, y) + INPUT_SECTION_GAP;
    x += DrawJoystick(renderer, 1, x, y) + INPUT_SECTION_GAP;
    x += DrawPaddles(renderer, x, y);

    float lowerH = 2 * INPUT_ROW_HEIGHT + 8 * MATRIX_CELL + 4 + INPUT_ROW_HEIGHT;
    m_clientContentSize.x = (int)((std::max)(keyboardW, x - x0) + 2 * INPUT_MARGIN);
    m_clientContentSize.y = (int)(y - y0 + lowerH + 2 * INPUT_MARGIN);
    LayoutScrollbars();
}

void InputScreenWindow::SaveTokens(std::vector<std::string>& layoutTokens)
{
    layoutTokens.push_back("INPUTSCREEN");
}

bool InputScreenWindow::CreateFromLayoutTokens(WindowLayout* layout, const std::vector<std::string>& layoutTokens, size_t& idx)
{
    if (layoutTokens[idx] != "INPUTSCREEN")
        return false;

    idx++;
    layout->m_tabs.push_back(new InputScreenWindow());
    return true;
}
