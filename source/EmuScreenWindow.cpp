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
            if (testIcon(Icons::Zoom, m_clientArea.x + 76, m_clientArea.y + 12))
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


static const char* s_zoomText[] = { "Auto", "2x", "4x", "8x" };
void EmuScreenWindow::Paint(SDL_Renderer* renderer, const Recti& dirtyArea)
{
    bool fixed = (m_zoom != 0);
    int level = 1 << m_zoom;
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
    bool isPal = Application::Instance().IsPAL();
    if (m_viceTexture)
    {
        int lineCycles = isPal ? 63 : 65;
        float textureVScale = isPal ? (float)m_viceTextureHeight * 1.07f : (float)m_viceTextureHeight * 1.07f * 271.0f / 246.0f;

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

        if (ViceBridge::Instance().HasViceStopped())
        {
            int rasterX = (viceState.m_rasterCycle - 14) * 8;
            if (rasterX >= 0 && rasterX < 384 && viceState.m_rasterLine >= 16)
            {
                float pixelWidth = body.w / m_viceTextureWidth;
                float pixelHeight = body.h / m_viceTextureHeight;

                SDL_FRect cycleArea{ body.x + rasterX * pixelWidth, body.y + pixelHeight * (viceState.m_rasterLine - 16), pixelWidth * 8.0f, pixelHeight };
                SDL_SetRenderDrawColor(m_renderer, 255, 255, 0, 128);
                SDL_RenderRect(m_renderer, &cycleArea);
            }
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
    ir.DrawIcon(renderer, Icons::Zoom, m_clientArea.x + 76, m_clientArea.y + 12);

    if (highlight.m_highlight == WindowHighlightType::EmuScreenIcon)
    {
        int ix = highlight.m_area.x + 8;
        int iy = highlight.m_area.y + 8;
        ir.DrawIcon(renderer, Icons::Highlight, ix, iy);
    }

    // draw emulator state
    auto& fr = FontRenderer::Instance();
    SDL_Color stateCol{ 255,255,255,255 };
    auto& vb = ViceBridge::Instance();
    int pads = vb.GetControllerCount();
    std::string joyText = (pads == 0 && !vb.IsMouseControllerMode()) ? std::string("-") : std::string(ControllerModeName(vb.GetControllerMode()));
    if (vb.IsMouseControllerMode())
        joyText += vb.IsMouseCaptured() ? " (Ctrl+M / middle click releases mouse)" : " (click screen to capture mouse)";
    std::string text = std::format("PC {:4x} ROW {:03x} COL {:3d} A {:2x} X {:2x} Y {:2x} Flags {}{}.{}{}{}{}{} Zoom {} Mode {}{} Pad {}",
        viceState.m_pc, viceState.m_rasterLine, viceState.m_rasterCycle, viceState.m_acc, viceState.m_x, viceState.m_y,
        viceState.m_flags & 128 ? 'N' : 'n',
        viceState.m_flags & 64 ? 'V' : 'v',
        viceState.m_flags & 16 ? 'B' : 'b',
        viceState.m_flags & 8 ? 'D' : 'd',
        viceState.m_flags & 4 ? 'I' : 'i',
        viceState.m_flags & 2 ? 'Z' : 'z',
        viceState.m_flags & 1 ? 'C' : 'c',
        s_zoomText[m_zoom],
        isPal ? "PAL" : "NTSC",
        m_warpMode ? " WARP" : "",
        joyText);
    fr.RenderText(renderer, text, stateCol, m_clientArea.x + 100, m_clientArea.y, FontType::UI);
}

bool EmuScreenWindow::HandleEvent(SDL_Event* e)
{
    switch (e->type)
    {
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
        {
            m_shift = (e->key.mod & SDL_KMOD_SHIFT);

            // Ctrl+W (warp), Ctrl+J (swap joystick port) and Ctrl+M (release a captured mouse) are MAD64 shortcuts; every
            // other Ctrl combination goes to the C64, as left Ctrl is the C64's CTRL key
            if ((e->key.mod & SDL_KMOD_LCTRL) && (e->key.scancode == SDL_SCANCODE_W || e->key.scancode == SDL_SCANCODE_J
                || (e->key.scancode == SDL_SCANCODE_M && ViceBridge::Instance().IsMouseCaptured())))
            {
                if (e->type == SDL_EVENT_KEY_DOWN && !e->key.repeat)
                {
                    if (e->key.scancode == SDL_SCANCODE_W)
                    {
                        m_warpMode = !m_warpMode;
                        auto cmd = new VBC_SetWarpMode;
                        cmd->m_enable = m_warpMode;
                        ViceBridge::Instance().SendMad2Vice(cmd);
                    }
                    else if (e->key.scancode == SDL_SCANCODE_J)
                    {
                        // swap which control port the controller drives
                        Application::Instance().Vice_SwapControllerPorts();
                    }
                    else
                    {
                        ViceBridge::Instance().ReleaseMouse();
                    }
                }
                return true;
            }

            int r2 = -1;
            int c2 = -1;
            int row = 0;
            int column = 0;
            bool cancelShift = false;

            // Keys are mapped by the character printed on a (US) PC keyboard rather than by position, so where shift
            // gives a different character on each machine the C64 key that types it is used instead:
            //  - c64(row, col)              the C64 key on its own
            //  - c64Unshifted(row, col)     with the C64's shift released (the PC shift is held for a C64 unshifted key)
            //  - c64Shifted(row, col)       with the C64's left shift added (a PC unshifted key types a C64 shifted one)
            bool shifted = (e->key.mod & SDL_KMOD_SHIFT) != 0;
            auto c64 = [&](int r, int c) { row = r; column = c; };
            auto c64Unshifted = [&](int r, int c) { row = r; column = c; cancelShift = true; };
            auto c64Shifted = [&](int r, int c) { row = r; column = c; r2 = 1; c2 = 7; };

            switch (e->key.scancode)
            {
                case SDL_SCANCODE_A:         row = 1; column = 2; break;
                case SDL_SCANCODE_B:         row = 3; column = 4; break;
                case SDL_SCANCODE_C:         row = 2; column = 4; break;
                case SDL_SCANCODE_D:         row = 2; column = 2; break;
                case SDL_SCANCODE_E:         row = 1; column = 6; break;
                case SDL_SCANCODE_F:         row = 2; column = 5; break;
                case SDL_SCANCODE_G:         row = 3; column = 2; break;
                case SDL_SCANCODE_H:         row = 3; column = 5; break;
                case SDL_SCANCODE_I:         row = 4; column = 1; break;
                case SDL_SCANCODE_J:         row = 4; column = 2; break;
                case SDL_SCANCODE_K:         row = 4; column = 5; break;
                case SDL_SCANCODE_L:         row = 5; column = 2; break;
                case SDL_SCANCODE_M:         row = 4; column = 4; break;
                case SDL_SCANCODE_N:         row = 4; column = 7; break;
                case SDL_SCANCODE_O:         row = 4; column = 6; break;
                case SDL_SCANCODE_P:         row = 5; column = 1; break;
                case SDL_SCANCODE_Q:         row = 7; column = 6; break;
                case SDL_SCANCODE_R:         row = 2; column = 1; break;
                case SDL_SCANCODE_S:         row = 1; column = 5; break;
                case SDL_SCANCODE_T:         row = 2; column = 6; break;
                case SDL_SCANCODE_U:         row = 3; column = 6; break;
                case SDL_SCANCODE_V:         row = 3; column = 7; break;
                case SDL_SCANCODE_W:         row = 1; column = 1; break;
                case SDL_SCANCODE_X:         row = 2; column = 7; break;
                case SDL_SCANCODE_Y:         row = 3; column = 1; break;
                case SDL_SCANCODE_Z:         row = 1; column = 4; break;
                // number row. Shift+1/3/4/5 give the same symbol on both; the rest differ
                case SDL_SCANCODE_1:         row = 7; column = 0; break;
                case SDL_SCANCODE_2:         if (shifted) c64Unshifted(5, 6); else c64(7, 3); break;    // '@' has its own key
                case SDL_SCANCODE_3:         row = 1; column = 0; break;
                case SDL_SCANCODE_4:         row = 1; column = 3; break;
                case SDL_SCANCODE_5:         row = 2; column = 0; break;
                case SDL_SCANCODE_6:         if (shifted) c64Unshifted(6, 6); else c64(2, 3); break;    // '^' is the up arrow key
                case SDL_SCANCODE_7:         if (shifted) c64(2, 3); else c64(3, 0); break;             // '&' is C64 shift+6
                case SDL_SCANCODE_8:         if (shifted) c64Unshifted(6, 1); else c64(3, 3); break;    // '*' has its own key
                case SDL_SCANCODE_9:         if (shifted) c64(3, 3); else c64(4, 0); break;             // '(' is C64 shift+8
                case SDL_SCANCODE_0:         if (shifted) c64(4, 0); else c64(4, 3); break;             // ')' is C64 shift+9

                // numeric keypad
                case SDL_SCANCODE_KP_0:      c64(4, 3); break;
                case SDL_SCANCODE_KP_1:      c64(7, 0); break;
                case SDL_SCANCODE_KP_2:      c64(7, 3); break;
                case SDL_SCANCODE_KP_3:      c64(1, 0); break;
                case SDL_SCANCODE_KP_4:      c64(1, 3); break;
                case SDL_SCANCODE_KP_5:      c64(2, 0); break;
                case SDL_SCANCODE_KP_6:      c64(2, 3); break;
                case SDL_SCANCODE_KP_7:      c64(3, 0); break;
                case SDL_SCANCODE_KP_8:      c64(3, 3); break;
                case SDL_SCANCODE_KP_9:      c64(4, 0); break;
                case SDL_SCANCODE_KP_PERIOD: c64(5, 4); break;
                case SDL_SCANCODE_KP_PLUS:   c64(5, 0); break;
                case SDL_SCANCODE_KP_MINUS:  c64(5, 3); break;
                case SDL_SCANCODE_KP_MULTIPLY: c64(6, 1); break;
                case SDL_SCANCODE_KP_ENTER:  c64(0, 1); break;

                // editing keys
                case SDL_SCANCODE_HOME:      c64(6, 3); break;          // CLR/HOME (shift gives CLR)
                case SDL_SCANCODE_DELETE:    c64(0, 0); break;          // DEL (the C64 has no forward delete)
                case SDL_SCANCODE_INSERT:    c64Shifted(0, 0); break;   // INST is shift+DEL

                case SDL_SCANCODE_LEFT:   r2 = 1; c2 = 7; row = 0; column = 2; break;
                case SDL_SCANCODE_RIGHT: row = 0; column = 2; break;
                case SDL_SCANCODE_UP: r2 = 1; c2 = 7; row = 0; column = 7; break;
                case SDL_SCANCODE_DOWN: row = 0; column = 7; break;
                case SDL_SCANCODE_TAB: row = 7; column = 7; break;
                case SDL_SCANCODE_SPACE:     row = 7; column = 4; break;
                case SDL_SCANCODE_RETURN:    row = 0; column = 1; break;
                case SDL_SCANCODE_BACKSPACE: row = 0; column = 0; break; // DEL
                case SDL_SCANCODE_ESCAPE:    row = 7; column = 7; break; // RUN/STOP
                case SDL_SCANCODE_LSHIFT:    row = 1; column = 7; break;
                case SDL_SCANCODE_RSHIFT:    row = 6; column = 4; break;
                case SDL_SCANCODE_LCTRL:     row = 7; column = 2; break; // CTRL
                case SDL_SCANCODE_LALT:      row = 7; column = 5; break; // C=
                case SDL_SCANCODE_F1:        row = 0; column = 4; break;
                case SDL_SCANCODE_F2:        r2 = 1; c2 = 7; row = 0; column = 4; break;
                case SDL_SCANCODE_F3:        row = 0; column = 5; break;
                case SDL_SCANCODE_F4:        r2 = 1; c2 = 7; row = 0; column = 5; break;
                case SDL_SCANCODE_F5:        row = 0; column = 6; break;
                case SDL_SCANCODE_F6:        r2 = 1; c2 = 7; row = 0; column = 6; break;
                case SDL_SCANCODE_F7:        row = 0; column = 3; break;
                case SDL_SCANCODE_F8:        r2 = 1; c2 = 7; row = 0; column = 3; break;

                case SDL_SCANCODE_COMMA:     row = 5; column = 7; break;
                case SDL_SCANCODE_PERIOD:    row = 5; column = 4; break;
                case SDL_SCANCODE_SLASH:     row = 6; column = 7; break; // shift gives '?' on both
                case SDL_SCANCODE_KP_DIVIDE: row = 6; column = 7; break;
                case SDL_SCANCODE_EQUALS:
                {
                    if (e->key.mod & SDL_KMOD_SHIFT)
                    {
                        // PLUS key
                        cancelShift = true;
                        row = 5; column = 0; break;
                    }
                    else
                    {
                        row = 6; column = 5; break;
                    }
                }
                break;
                case SDL_SCANCODE_MINUS:     if (shifted) c64Unshifted(7, 1); else c64(5, 3); break;    // '_' is PETSCII's left arrow

                // punctuation
                case SDL_SCANCODE_SEMICOLON: if (shifted) c64Unshifted(5, 5); else c64(6, 2); break;    // ':' has its own key
                case SDL_SCANCODE_APOSTROPHE: if (shifted) c64(7, 3); else c64Shifted(3, 0); break;     // '"' is shift+2, ''' is shift+7
                case SDL_SCANCODE_LEFTBRACKET: c64Shifted(5, 5); break;     // '[' is shift+':'
                case SDL_SCANCODE_RIGHTBRACKET: c64Shifted(6, 2); break;    // ']' is shift+';'
                case SDL_SCANCODE_BACKSLASH: c64(6, 0); break;              // PETSCII has the pound sign where ASCII has '\'
                case SDL_SCANCODE_GRAVE:     c64(7, 1); break;              // left arrow

                default:
                    return false;
            }

            // Forward transitions only. The emulated machine handles key repeat.
            if (e->key.repeat)
                return true;

            // Release whatever C64 key this PC key pressed, even if the modifiers changed in between
            // (e.g. shift let go before the 8 of a shift+8 '*'), so no C64 key is left held down.
            int sc = (int)e->key.scancode;
            if (sc >= 0 && sc < SDL_SCANCODE_COUNT)
            {
                auto& held = m_heldKeys[sc];
                if (e->key.down)
                {
                    held = { true, row, column, r2, c2, cancelShift };
                }
                else if (held.m_held)
                {
                    row = held.m_row;
                    column = held.m_column;
                    r2 = held.m_r2;
                    c2 = held.m_c2;
                    cancelShift = held.m_cancelShift;
                    held.m_held = false;
                }
            }

            auto* cmd = new VBC_Key;
            cmd->m_row = row;
            cmd->m_column = column;
            cmd->m_r2 = r2;
            cmd->m_c2 = c2;
            cmd->m_pressed = e->key.down;
            cmd->m_cancelShift = cancelShift;
            ViceBridge::Instance().SendMad2Vice(cmd);
        }
        return true;


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
                        if (m_shift)
                        {
                            ViceBridge::Instance().MultiStep();
                        }
                        else
                        {
                            ViceBridge::Instance().SingleStep();
                        }
                        break;

                    case Icons::Zoom:
                        m_zoom = (m_zoom + 1) % 4;
                        break;
                }
            }
            else if (e->button.button == SDL_BUTTON_LEFT && ViceBridge::Instance().IsMouseControllerMode())
            {
                // MousePad modes: clicking the screen hands the mouse to the paddles until released
                ViceBridge::Instance().CaptureMouse(SDL_GetWindowFromID(e->button.windowID));
                return true;
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


