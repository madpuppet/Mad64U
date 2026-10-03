#include "common.h"
#include "SourceFileWindow.h"
#include "SourceFileManager.h"
#include "SourceFile.h"
#include "Application.h"
#include "SourceFileCmdBuffer.h"
#include "LogManager.h"
#include "SearchWindow.h"
#include "Settings.h"
#include "ViceBridge.h"
#include <filesystem>

SourceFileWindow::SourceFileWindow(SourceFile* file) : m_sourceFile(file)
{
    if (file->m_path.empty())
    {
        m_name = "<new-file>";
    }
    else
    {
        std::filesystem::path path = file->m_path;
        m_name = path.filename().string();
        m_clientContentSize.x = 256;
        m_clientContentSize.y = (int)file->m_lines.size() * LINE_HEIGHT;
    }

    m_charWidth = FontRenderer::Instance().GetCharactorWidth(FontType::Text, (u32)' ');
}

void SourceFileWindow::ClampCursor()
{
    m_cursor.y = Min(m_cursor.y, (int)m_sourceFile->m_lines.size() - 1);
    m_cursor.x = Min(m_cursor.x, (int)m_sourceFile->m_lines[m_cursor.y]->m_chars.size());
}

Recti SourceFileWindow::CalcCursorArea()
{
    auto& fr = FontRenderer::Instance();
    Recti rect = Recti{ 0,2,3,LINE_HEIGHT };
    if (!m_sourceFile->m_lines.empty())
    {
        ClampCursor();
        auto line = m_sourceFile->m_lines[m_cursor.y];
        int cmax = Min(m_cursor.x, (int)line->m_chars.size());
        int x = 0;
        int chIdx = 0;
        for (int i = 0; i < cmax; i++)
        {
            u32 ch = line->m_chars[i];
            if (ch == (u32)'\t')
            {
                chIdx = ((chIdx / TAB_SIZE) + 1) * TAB_SIZE;
            }
            else
            {
                chIdx++;
            }
            x = chIdx * m_charWidth;
        }
        rect.x = x;
        rect.y = m_cursor.y * LINE_HEIGHT + 2;
        if (m_overwrite)
        {
            int cindex = 0;
            if (cmax < line->m_chars.size())
            {
                u32 ch = line->m_chars[cmax];
                if (ch == (u32)'\t')
                    cindex = ((cindex / TAB_SIZE) + 1) * TAB_SIZE;
                else
                    cindex++;
            }
            rect.w = chIdx * m_charWidth - cindex * m_charWidth;
        }
        rect.h = LINE_HEIGHT;
    }
    return rect;
}

int SourceFileWindow::CalcXPos(int x, int y)
{
    auto& fr = FontRenderer::Instance();
    auto line = m_sourceFile->m_lines[y];

    x = Min(x, (int)line->m_chars.size());

    int chIdx = 0;
    for (int i = 0; i < x; i++)
    {
        u32 ch = line->m_chars[i];
        if (ch == (u32)'\t')
            chIdx = ((chIdx / TAB_SIZE) + 1) * TAB_SIZE;
        else
            chIdx++;
    }
    return chIdx * m_charWidth;
}

void SourceFileWindow::Paint(SDL_Renderer* renderer, const Recti& dirtyArea)
{
    bool isActiveVice = gViceBridge->GetActiveFileID() == m_sourceFile->m_fileID;
    auto& viceState = gViceBridge->GetViceState();

    bool showLineNumbers = Settings::Instance().GetBool(SETTING_SHOW_LINES);
    bool showDisassembly = Settings::Instance().GetBool(SETTING_SHOW_BYTES);

    auto& tp = Application::Instance().GetThemeProperties();
    auto window = WindowManager::Instance().GetActiveWindowBase();
    if (window == this)
        tp.SetRenderDrawColor(renderer, ThemeColor::SourceBackgroundSelected);
    else
        tp.SetRenderDrawColor(renderer, ThemeColor::SourceBackground);

    SDL_FRect body = m_clientArea.AsSDLFRect();
    SDL_RenderFillRect(renderer, &body);

    auto& ir = IconRenderer::Instance();
    auto& fr = FontRenderer::Instance();
    int firstLine = Max(m_clientContentOffset.y / LINE_HEIGHT, 0);
    int lastLine = Min(firstLine + m_clientArea.h / LINE_HEIGHT, (int)m_sourceFile->m_lines.size());
    int y = firstLine * LINE_HEIGHT;
    int xBase = m_clientArea.x - m_clientContentOffset.x + BORDER_MARGIN;
    int yBase = m_clientArea.y - m_clientContentOffset.y + BORDER_MARGIN;

    SDL_FRect highlightRect{ (float)xBase, (float)(yBase + m_cursor.y * LINE_HEIGHT), (float)m_clientArea.w, (float)LINE_HEIGHT };
    tp.SetRenderDrawColor(renderer, ThemeColor::HighlightLine);
    SDL_RenderFillRect(renderer, &highlightRect);

    // breakpoints
    for (int i = firstLine; i < lastLine; i++)
    {
        auto line = m_sourceFile->m_lines[i];
        if (line->m_breakpointID != 0)
        {
            ir.DrawIcon(renderer, Icons::Breakpoint, xBase + 5, (int)(yBase + i * LINE_HEIGHT + LINE_HEIGHT * 0.5f - 2));
        }
    }

    int bodyX = m_clientArea.x;
    if (showLineNumbers)
    {
        SDL_FRect lnBody{ (float)bodyX, body.y, (float)(m_lineNmbrOffset - BORDER_MARGIN), body.h };
        SDL_FRect lnBar{ lnBody.x + m_lineNmbrOffset - BORDER_MARGIN, lnBody.y, BORDER_MARGIN, lnBody.h };
        tp.SetRenderDrawColor(renderer, ThemeColor::SourceBackground);
        SDL_RenderFillRect(renderer, &lnBar);
        tp.SetRenderDrawColor(renderer, ThemeColor::WindowEdgeDark);
        SDL_RenderLine(renderer, lnBar.x, lnBar.y, lnBar.x, lnBar.y + lnBar.h);
        tp.SetRenderDrawColor(renderer, ThemeColor::WindowEdgeLight);
        SDL_RenderLine(renderer, lnBar.x + lnBar.w, lnBar.y, lnBar.x + lnBar.w, lnBar.y + lnBar.h);

        SDL_Rect oldArea;
        SDL_GetRenderClipRect(renderer, &oldArea);
        SDL_Rect disClipArea{ (int)lnBody.x, (int)lnBody.y, (int)lnBody.w, (int)lnBody.h };
        SDL_SetRenderClipRect(renderer, &disClipArea);

        for (int i = firstLine; i < lastLine; i++)
        {
            int ly = yBase + i * LINE_HEIGHT;
            int lx = xBase;
            fr.RenderText(renderer, std::format("{:5d}", i), tp.m_colors[(int)ThemeColor::TextString], lx, ly, FontType::Text);
            ly += LINE_HEIGHT;
        }

        SDL_SetRenderClipRect(renderer, &oldArea);
        xBase += m_lineNmbrOffset;
        bodyX += m_lineNmbrOffset;
    }

    if (showDisassembly)
    {
        auto dis = SourceFileManager::Instance().GetDisassembly(m_sourceFile);
        if (dis)
        {
            SDL_FRect disBody{ (float)bodyX, body.y, (float)(m_disOffset - BORDER_MARGIN), body.h };
            SDL_FRect disBar{ disBody.x + m_disOffset - BORDER_MARGIN, disBody.y, BORDER_MARGIN, disBody.h };

            tp.SetRenderDrawColor(renderer, ThemeColor::SourceBackground);
            SDL_RenderFillRect(renderer, &disBar);
            tp.SetRenderDrawColor(renderer, ThemeColor::WindowEdgeDark);
            SDL_RenderLine(renderer, disBar.x, disBar.y, disBar.x, disBar.y + disBar.h);
            tp.SetRenderDrawColor(renderer, ThemeColor::WindowEdgeLight);
            SDL_RenderLine(renderer, disBar.x + disBar.w, disBar.y, disBar.x + disBar.w, disBar.y + disBar.h);

            SDL_Rect oldArea;
            SDL_GetRenderClipRect(renderer, &oldArea);
            SDL_Rect disClipArea{ (int)disBody.x, (int)disBody.y, (int)disBody.w, (int)disBody.h };
            SDL_SetRenderClipRect(renderer, &disClipArea);

            for (int i = firstLine; i < lastLine; i++)
            {
                auto srcLine = m_sourceFile->m_lines[i];
                if (srcLine->m_assembledLine < 0 || srcLine->m_assembledLine >= dis->m_lines.size())
                    continue;

                auto& line = dis->m_lines[srcLine->m_assembledLine];
                if (line.m_addressLength == 0)
                    continue;

                u32 offsetAddress = line.m_addressStart - dis->m_info->m_memoryAddress;
                if (offsetAddress > dis->m_info->m_memoryLength)
                    continue;

                int ly = yBase + i * LINE_HEIGHT;
                int lx = xBase;

                if (isActiveVice && (int)viceState.m_pc >= (int)line.m_addressStart && (int)viceState.m_pc < (int)line.m_addressStart + (int)line.m_addressLength)
                {
                    SDL_FRect highlight{ disBody.x, (float)ly, disBody.w, LINE_HEIGHT };
                    if (gViceBridge->HasViceStopped())
                        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 128);
                    else
                        SDL_SetRenderDrawColor(renderer, 128, 128, 128, 128);
                    SDL_RenderFillRect(renderer, &highlight);
                }

                u32 bytes = Min(line.m_addressLength, 3);
                fr.RenderText(renderer, std::format("{:04x}", line.m_addressStart), tp.m_colors[(int)ThemeColor::TextOperator], lx, ly, FontType::Text);
                lx += 50;
                for (u32 b = 0; b < bytes; b++)
                {
                    u8 byte = dis->m_info->m_memory[offsetAddress + b];
                    fr.RenderText(renderer, std::format("{:02x}", byte), tp.m_colors[(int)ThemeColor::TextComment], lx, ly, FontType::Text);
                    lx += 24;
                }
                ly += LINE_HEIGHT;
            }
            SDL_SetRenderClipRect(renderer, &oldArea);
            xBase += m_disOffset;
            bodyX += m_disOffset;
        }
    }

    int maxWidth = 0;

    // draw selection
    if (m_marked)
    {
        Vec2i topPos = (m_cursor.y < m_markStart.y || (m_cursor.y == m_markStart.y && m_cursor.x < m_markStart.x)) ? m_cursor : m_markStart;
        Vec2i bottomPos = (m_cursor.y < m_markStart.y || (m_cursor.y == m_markStart.y && m_cursor.x < m_markStart.x)) ? m_markStart : m_cursor;
        int markYStart = Max(firstLine, topPos.y);
        int markYEnd = Min(lastLine, bottomPos.y+1);
        tp.SetRenderDrawColor(renderer, ThemeColor::TextHighlight);
        for (int i = markYStart; i < markYEnd; i++)
        {
            auto line = m_sourceFile->m_lines[i];
            int xStart = 0;
            int xEnd = CalcXPos((int)line->m_chars.size(), i) + 4;
            if (i == topPos.y)
            {
                xStart = CalcXPos(topPos.x, topPos.y);
            }
            if (i == bottomPos.y)
            {
                xEnd = CalcXPos(bottomPos.x, bottomPos.y);
            }
            SDL_FRect rect{(float)(xBase + xStart), (float)(yBase + i * LINE_HEIGHT + 2), (float)((xBase + xEnd) - (xBase + xStart)), (float)LINE_HEIGHT};
            SDL_RenderFillRect(renderer, &rect);
        }
    }

    for (int i = firstLine; i < lastLine; i++)
    {
        auto line = m_sourceFile->m_lines[i];
        if (line->m_fragmentsDirty)
            line->BuildFragments(renderer, m_sourceFile->m_sourceType);
        for (auto& fragment : line->m_fragments)
        {
            auto& col = tp.m_colors[(int)ThemeColor::TextGeneral + (int)fragment.m_fragType];
            fr.RenderText(renderer, fragment.m_chars, col, xBase + fragment.m_area.x, yBase + y, FontType::Text);
            maxWidth = Max(fragment.m_area.x + fragment.m_area.w, maxWidth);
        }
        y += LINE_HEIGHT;
    }

    m_clientContentSize.x = maxWidth + 32;

    if (m_breakpointFlash > 0.0f)
    {
        SDL_FRect body = m_clientArea.AsSDLFRect();
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, (int)(77.0f * m_breakpointFlash));
        SDL_RenderFillRect(renderer, &body);
    }

    // draw cursor
    SDL_FRect cursorRect = CalcCursorArea().AsSDLFRect();
    cursorRect.x += (float)xBase;
    cursorRect.y += (float)yBase;
    SDL_Color col = tp.m_colors[(int)ThemeColor::Cursor];
    if (WindowManager::Instance().GetActiveWindowBase() == this)
    {
        col.a = (int)(cosf(m_animTime * 2.0f * SDL_PI_F) * 127) + 128;
    }
    else
    {
        col.a = 160;
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, col.a);
    SDL_RenderFillRect(renderer, &cursorRect);
}

void SourceFileWindow::Close()
{
    SDL_MessageBoxButtonData buttons[] =
    {
        { SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 2, "Cancel" },
        { SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "Yes" },
        { 0, 0, "Cancel" }
    };

    SDL_MessageBoxData data =
    {
        SDL_MESSAGEBOX_INFORMATION,
        nullptr,               // parent window
        "Confirm",
        "Do you want to save changes?",
        SDL_arraysize(buttons),
        buttons,
        nullptr                // color scheme
    };

    int buttonid = -1;

    if (m_sourceFile->m_cmdBuffer->m_cmdIndex > 0)
    {
        if (SDL_ShowMessageBox(&data, &buttonid))
        {
            switch (buttonid)
            {
                case 2:
                    break;

                case 1:
                    SourceFileManager::Instance().SaveFile(m_sourceFile);
                    SourceFileManager::Instance().CloseFile(m_sourceFile);
                    break;

                case 0:
                    SourceFileManager::Instance().CloseFile(m_sourceFile);
                    break;
            }
        }
    }
    else
    {
        SourceFileManager::Instance().CloseFile(m_sourceFile);
    }
}

bool SourceFileWindow::Tick()
{
    float deltaTime = WINDOW_TICK_MS * (1.0f / 1000.0f);

    m_animTime += deltaTime;
    if (m_animTime > 1.0f)
        m_animTime -= 1.0f;

    if (m_breakpointFlash > 0.0f)
    {
        m_breakpointFlash = Max(m_breakpointFlash - deltaTime*2.0f, 0.0f);
    }
    return true;
}

void SourceFileWindow::CalcXYFromClientPos(int x, int y, int& col, int& row)
{
    bool showLineNumbers = true;
    bool showDisassembly = true;

    auto &fr = FontRenderer::Instance();
    int xBase = m_clientArea.x - m_clientContentOffset.x + BORDER_MARGIN;
    int yBase = m_clientArea.y - m_clientContentOffset.y + BORDER_MARGIN;

    if (showLineNumbers)
        xBase += m_lineNmbrOffset;
    auto dis = SourceFileManager::Instance().GetDisassembly(m_sourceFile);
    if (dis && showDisassembly)
        xBase += m_disOffset;

    row = Clamp((y - yBase) / LINE_HEIGHT, 0, (int)m_sourceFile->m_lines.size() - 1);

    auto line = m_sourceFile->m_lines[row];
    int x1 = 0;
    int x2 = 0;
    x -= xBase;
    col = 0;
    int chIdx = 0;
    for (auto ch : line->m_chars)
    {
        if (ch == (u32)'\t')
        {
            chIdx = ((chIdx / TAB_SIZE) + 1) * TAB_SIZE;
        }
        else
        {
            chIdx++;
        }
        x2 = chIdx * m_charWidth;
        if (x < x2-3)
            break;
        col++;
        x1 = x2;
    }
}

bool SourceFileWindow::HandleEvent(SDL_Event* e)
{
    switch (e->type)
    {
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        {
            if (e->button.button == 1)
            {
                int row, col;
                CalcXYFromClientPos((int)e->button.x, (int)e->button.y, col, row);
                MoveCursorXY(col, row);
                m_mouseLeftDown = true;

                bool moved = (m_mouseDownPos.x != col || m_mouseDownPos.y != row);
                m_mouseDownPos.x = col;
                m_mouseDownPos.y = row;

                if (!m_shiftDown)
                {
                    ClearMarking();
                }
                if (e->button.clicks > 1 && !moved)
                {
                    MarkCurrentWord();
                }
            }
            return true;
        }
        break;

        case SDL_EVENT_MOUSE_MOTION:
        {
            if (e->button.button == 1)
            {
                if (m_mouseLeftDown)
                {
                    int row, col;
                    CalcXYFromClientPos((int)e->button.x, (int)e->button.y, col, row);

                    if (!m_marking && (m_mouseDownPos.x != col || m_mouseDownPos.y != row))
                    {
                        StartMarking();
                    }

                    MoveCursorXY(col, row);
                    return true;
                }
            }
        }
        break;

        case SDL_EVENT_MOUSE_BUTTON_UP:
        {
            if (e->button.button == 1)
            {
                m_mouseLeftDown = false;
            }
        }
        break;

        case SDL_EVENT_KEY_DOWN:
            m_shiftDown = (e->key.mod & SDL_KMOD_SHIFT);
            switch (e->key.key)
            {
                case SDLK_F:
                    if (e->key.mod & SDL_KMOD_CTRL)
                    {
                        Log(LogGroup::Build, "CTRL F");

                        WindowMessageStruct msg;
                        WindowFindQuery query;
                        query.m_windowName = "Search";
                        msg.m_type = WindowMessage::Query_FindWindow;
                        msg.m_flags = WMF_Window | WMF_EarlyOut;
                        msg.m_query = &query;
                        WindowManager::Instance().Message(msg);

                        int count = (int) query.m_foundWindows.size();
                        Log(LogGroup::Build, "response {}  found {}", msg.m_response, count);

                        SearchWindow *searchWindow = nullptr;
                        if (msg.m_response == 0)
                        {
                            // open a new window
                            searchWindow = new SearchWindow;
                            WindowManager::Instance().AddWindow(searchWindow);
                        }
                        else
                        {
                            // check for a search window already targetting this source file
                            for (auto win : query.m_foundWindows)
                            {
                                if (win->GetSourceFile() == m_sourceFile)
                                    searchWindow = (SearchWindow*)win;
                            }
                            if (searchWindow == nullptr)
                                searchWindow = (SearchWindow*)query.m_foundWindows[0];
                        }

                        if (msg.m_layout)
                        {
                            msg.m_layout->ActivateWindow(searchWindow);
                        }
                        searchWindow->SetSearchActive();

                        if (m_marked && m_markStart.y == m_cursor.y)
                        {
                            // copy mark string into search window
                            auto line = m_sourceFile->m_lines[m_cursor.y];
                            std::string str(line->m_chars.begin() + m_markStart.x, line->m_chars.begin() + m_cursor.x);
                            searchWindow->SetSearchText(str);
                            searchWindow->Search();
                        }
                    }
                    break;

                case SDLK_S:
                    if (e->key.mod & SDL_KMOD_CTRL)
                    {
                        SourceFileManager::Instance().SaveFile(m_sourceFile);

                        if (m_sourceFile->m_sourceType == SourceType::Asm)
                            SourceFileManager::Instance().Compile(m_sourceFile, false);
                    }
                    break;


                case SDLK_F10:
                {
                    // find next line addr
                    // put temp breakpoint on it
                    if (m_sourceFile->m_stepOverAddr == -1 && m_sourceFile->m_stepOverBreakpointID == 0)
                    {
                        auto dis = SourceFileManager::Instance().GetDisassembly(m_sourceFile);
                        if (dis)
                        {
                            // find the current stopped line
                            auto& viceState = gViceBridge->GetViceState();
                            int addr = viceState.m_pc;
                            for (int l = 0; l < m_sourceFile->m_lines.size(); l++)
                            {
                                auto ln = m_sourceFile->m_lines[l];
                                if (ln->m_assembledLine >= 0 && ln->m_assembledLine < dis->m_lines.size())
                                {
                                    auto& disln = dis->m_lines[ln->m_assembledLine];
                                    int addrStart = disln.m_addressStart;
                                    int addrEnd = addrStart + disln.m_addressLength;
                                    if (addr >= addrStart && addr < addrEnd)
                                    {
                                        // is this a JSR?
                                        u32 offsetAddress = disln.m_addressStart - dis->m_info->m_memoryAddress;
                                        if (offsetAddress > dis->m_info->m_memoryLength)
                                            return true;

                                        u8 byte = dis->m_info->m_memory[offsetAddress];
                                        if (byte == 0x20)
                                        {
                                            // find next line...
                                            for (int ll = l + 1; ll < m_sourceFile->m_lines.size(); ll++)
                                            {
                                                auto stopln = m_sourceFile->m_lines[ll];
                                                if (stopln->m_assembledLine >= 0 && stopln->m_assembledLine < dis->m_lines.size())
                                                {
                                                    int breakAddr = dis->m_lines[stopln->m_assembledLine].m_addressStart;
                                                    auto cmd = new VBC_SetBreakpoint;
                                                    cmd->m_fileID = m_sourceFile->m_fileID;
                                                    cmd->m_lineID = ln->m_uniqueID;
                                                    cmd->m_addr = breakAddr;
                                                    gViceBridge->SendMad2Vice(cmd);
                                                    gViceBridge->Continue();
                                                    m_sourceFile->m_stepOverAddr = breakAddr;
                                                    return true;
                                                }
                                            }
                                        }
                                        else
                                        {
                                            gViceBridge->SingleStep();
                                            return true;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                return true;

                case SDLK_F11:
                    if (e->key.mod & SDL_KMOD_SHIFT)
                        gViceBridge->MultiStep();
                    else
                        gViceBridge->SingleStep();
                    return true;

                case SDLK_F5:
                    {
                        if (e->key.mod & SDL_KMOD_CTRL)
                        {
                            // find output file
                            std::filesystem::path path = m_sourceFile->m_path;
                            std::filesystem::path output = path.parent_path() / "out" / path.stem();
                            std::filesystem::path outputPrg = output.replace_extension("prg");
                            std::filesystem::path outputD64 = output.replace_extension("d64");
                            if (std::filesystem::exists(outputPrg))
                            {
                                SourceFileManager::Instance().Run(outputPrg);
                                gViceBridge->SetActiveFileID(m_sourceFile->m_fileID);
                            }
                            else if (std::filesystem::exists(outputD64))
                            {
                                SourceFileManager::Instance().Run(outputD64);
                                gViceBridge->SetActiveFileID(m_sourceFile->m_fileID);
                            }
                            else
                            {
                                SourceFileManager::Instance().Compile(m_sourceFile, true);
                                gViceBridge->SetActiveFileID(m_sourceFile->m_fileID);
                            }
                        }
                        else
                        {
                            bool isActiveVice = gViceBridge->GetActiveFileID() == m_sourceFile->m_fileID;
                            bool viceStopped = gViceBridge->HasViceStopped();
                            if (isActiveVice && viceStopped)
                            {
                                gViceBridge->Continue();
                            }
                        }
                    }
                    break;

                case SDLK_F9:
                    ToggleBreakpoint(m_cursor.y);
                    break;

                case SDLK_TAB:
                    InsertTextAtCursor("\t");
                    break;
                case SDLK_LEFT:
                    if (e->key.mod & SDL_KMOD_SHIFT)
                        StartMarking();
                    else
                        ClearMarking();
                    MoveCursorLeft();
                    return true;
                case SDLK_RIGHT:
                    if (e->key.mod & SDL_KMOD_SHIFT)
                        StartMarking();
                    else
                        ClearMarking();
                    MoveCursorRight();
                    return true;
                case SDLK_UP:
                    if (e->key.mod & SDL_KMOD_SHIFT)
                        StartMarking();
                    else
                        ClearMarking();
                    MoveCursorUp();
                    return true;
                case SDLK_DOWN:
                    if (e->key.mod & SDL_KMOD_SHIFT)
                        StartMarking();
                    else
                        ClearMarking();
                    MoveCursorDown();
                    return true;
                case SDLK_PAGEUP:
                    if (e->key.mod & SDL_KMOD_SHIFT)
                        StartMarking();
                    else
                        ClearMarking();
                    MoveCursorPageUp();
                    return true;
                case SDLK_PAGEDOWN:
                    if (e->key.mod & SDL_KMOD_SHIFT)
                        StartMarking();
                    else
                        ClearMarking();
                    MoveCursorPageDown();
                    return true;
                case SDLK_HOME:
                    if (e->key.mod & SDL_KMOD_SHIFT)
                        StartMarking();
                    else
                        ClearMarking();
                    if (e->key.mod & SDL_KMOD_CTRL)
                        MoveCursorStartOfFile();
                    else
                        MoveCursorStartOfLine();
                    return true;
                case SDLK_END:
                    if (e->key.mod & SDL_KMOD_SHIFT)
                        StartMarking();
                    else
                        ClearMarking();
                    if (e->key.mod & SDL_KMOD_CTRL)
                        MoveCursorEndOfFile();
                    else
                        MoveCursorEndOfLine();
                    return true;
                case SDLK_BACKSPACE:
                    if (m_marked)
                        DeleteSelected();
                    else
                        DeleteCharBeforeCursor();
                    return true;
                case SDLK_DELETE:
                    if (m_marked)
                        DeleteSelected();
                    else
                        DeleteCharAfterCursor();
                    return true;
                case SDLK_RETURN:
                    if (m_marked)
                        DeleteSelected();
                    InsertNewLineAtCursor();
                    return true;
                case SDLK_C:
                    if (e->key.mod & SDL_KMOD_CTRL)
                    {
                        if (m_marked)
                        {
                            // copy
                            CopySelected();
                        }
                    }
                    return true;
                case SDLK_X:
                    if (e->key.mod & SDL_KMOD_CTRL)
                    {
                        if (m_marked)
                        {
                            // cut
                            CopySelected();
                            DeleteSelected();
                        }
                    }
                    return true;
                case SDLK_V:
                    if (e->key.mod & SDL_KMOD_CTRL)
                    {
                        // paste
                        DeleteSelected();
                        PasteSelected();
                    }
                    return true;
                case SDLK_Z:
                    if (e->key.mod & SDL_KMOD_CTRL)
                    {
                        if (e->key.mod & SDL_KMOD_SHIFT)
                        {
                            Vec2i cursor;
                            if (m_sourceFile->m_cmdBuffer->Execute(m_sourceFile, cursor))
                            {
                                m_cursor = cursor;
                                WindowManager::Instance().GetActiveWindowTree()->m_dirty = true;
                                
                            }
                        }
                        else
                        {
                            Vec2i cursor;
                            if (m_sourceFile->m_cmdBuffer->Revert(m_sourceFile, cursor))
                            {
                                m_cursor = cursor;
                                WindowManager::Instance().GetActiveWindowTree()->m_dirty = true;
                            }
                        }
                        m_marked = false;
                        m_marking = false;
                    }
            }
            break;

        case SDL_EVENT_KEY_UP:
            m_shiftDown = (e->key.mod & SDL_KMOD_SHIFT);
            break;

        case SDL_EVENT_TEXT_INPUT:
            DeleteSelected();
            InsertTextAtCursor(e->text.text);
            break;
    }
    return WindowBase::HandleEvent(e);
}

bool IsAlphaNumeric(char ch)
{
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || (ch == '_');
}

void SourceFileWindow::MarkCurrentWord()
{
    auto line = m_sourceFile->m_lines[m_mouseDownPos.y];
    int start = m_mouseDownPos.x;
    int end = m_mouseDownPos.x;
    int lineLength = (int)line->m_chars.length();
    if (start < lineLength)
    {
        bool isAlpha = IsAlphaNumeric(line->m_chars[start]);

        while (start > 0 && IsAlphaNumeric(line->m_chars[start - 1]) == isAlpha)
            start--;
        while (end < lineLength && IsAlphaNumeric(line->m_chars[end]) == isAlpha)
            end++;

        m_markStart.y = m_mouseDownPos.y;
        m_markStart.x = start;
        m_cursor.x = end;
        m_cursor.y = m_mouseDownPos.y;
        m_marked = true;
        m_marking = false;
    }
}

void SourceFileWindow::MoveCursorLeft()
{
    Assert(!m_sourceFile->m_lines.empty(), "Source File Can't be Empty!");

    if (m_cursor.x > 0)
    {
        m_cursor.x--;
        m_trackedColumn = m_cursor.x;
        m_animTime = 0.0f;
        MakeCursorVisible();
    }
    else if (m_cursor.y > 0)
    {
        m_cursor.y--;
        m_cursor.x = (int)m_sourceFile->m_lines[m_cursor.y]->m_chars.size();
        m_trackedColumn = m_cursor.x;
        m_animTime = 0.0f;
        MakeCursorVisible();
    }
}

void SourceFileWindow::MoveCursorRight()
{
    Assert(!m_sourceFile->m_lines.empty(), "Source File Can't be Empty!");

    if (m_cursor.x < m_sourceFile->m_lines[m_cursor.y]->m_chars.size())
    {
        m_cursor.x++;
        m_trackedColumn = m_cursor.x;
        m_animTime = 0.0f;
        MakeCursorVisible();
    }
    else if (m_cursor.y < m_sourceFile->m_lines.size()-1)
    {
        m_cursor.y++;
        m_cursor.x = 0;
        m_trackedColumn = m_cursor.x;
        m_animTime = 0.0f;
        MakeCursorVisible();
    }
}

void SourceFileWindow::MoveCursorUp()
{
    Assert(!m_sourceFile->m_lines.empty(), "Source File Can't be Empty!");

    if (m_cursor.y > 0)
    {
        m_cursor.y--;
        m_cursor.x = Min(m_trackedColumn, (int) m_sourceFile->m_lines[m_cursor.y]->m_chars.size());
        m_animTime = 0.0f;
        MakeCursorVisible();
    }
}

void SourceFileWindow::MoveCursorDown()
{
    Assert(!m_sourceFile->m_lines.empty(), "Source File Can't be Empty!");

    if (m_cursor.y < m_sourceFile->m_lines.size()-1)
    {
        m_cursor.y++;
        m_cursor.x = Min(m_trackedColumn, (int)m_sourceFile->m_lines[m_cursor.y]->m_chars.size());
        m_animTime = 0.0f;
        MakeCursorVisible();
    }
}

void SourceFileWindow::MoveCursorPageUp()
{
    Assert(!m_sourceFile->m_lines.empty(), "Source File Can't be Empty!");

    if (m_cursor.y > 0)
    {
        m_cursor.y = Max(m_cursor.y - m_clientArea.h / LINE_HEIGHT, 0);
        m_cursor.x = Min(m_trackedColumn, (int)m_sourceFile->m_lines[m_cursor.y]->m_chars.size());
        m_animTime = 0.0f;
        MakeCursorVisible();
    }
}

void SourceFileWindow::MoveCursorXY(int x, int y)
{
    m_cursor.x = x;
    m_cursor.y = y;
    m_trackedColumn = m_cursor.x;
    m_animTime = 0.0f;
    MakeCursorVisible();
}

void SourceFileWindow::MoveCursorPageDown()
{
    Assert(!m_sourceFile->m_lines.empty(), "Source File Can't be Empty!");

    if (m_cursor.y < m_sourceFile->m_lines.size() - 1)
    {
        m_cursor.y = Min(m_cursor.y + m_clientArea.h / LINE_HEIGHT, (int)m_sourceFile->m_lines.size()-1);
        m_cursor.x = Min(m_trackedColumn, (int)m_sourceFile->m_lines[m_cursor.y]->m_chars.size());
        m_animTime = 0.0f;
        MakeCursorVisible();
    }
}

void SourceFileWindow::MoveCursorStartOfFile()
{
    Assert(!m_sourceFile->m_lines.empty(), "Source File Can't be Empty!");

    m_cursor.x = 0;
    m_cursor.y = 0;
    m_trackedColumn = m_cursor.x;
    m_animTime = 0.0f;
    MakeCursorVisible();
}

void SourceFileWindow::MoveCursorEndOfFile()
{
    Assert(!m_sourceFile->m_lines.empty(), "Source File Can't be Empty!");

    m_cursor.y = (int)(m_sourceFile->m_lines.size()-1);
    m_cursor.x = (int)m_sourceFile->m_lines[m_cursor.y]->m_chars.size();
    m_trackedColumn = m_cursor.x;
    m_animTime = 0.0f;
    MakeCursorVisible();
}

void SourceFileWindow::MoveCursorStartOfLine()
{
    Assert(!m_sourceFile->m_lines.empty(), "Source File Can't be Empty!");

    m_cursor.x = 0;
    m_trackedColumn = m_cursor.x;
    m_animTime = 0.0f;
    MakeCursorVisible();
}

bool SourceFileWindow::IsModified()
{
    return m_sourceFile->m_cmdBuffer->IsModified();
}

void SourceFileWindow::MoveCursorEndOfLine()
{
    Assert(!m_sourceFile->m_lines.empty(), "Source File Can't be Empty!");

    m_cursor.x = (int)m_sourceFile->m_lines[m_cursor.y]->m_chars.size();
    m_trackedColumn = m_cursor.x;
    m_animTime = 0.0f;
    MakeCursorVisible();
}

void SourceFileWindow::MakeRowVisible(int row)
{
    int yOffset = row * LINE_HEIGHT;
    if (yOffset < m_clientContentOffset.y)
        m_clientContentOffset.y = yOffset;
    if (yOffset > (m_clientContentOffset.y + m_clientArea.h - LINE_HEIGHT * 2))
        m_clientContentOffset.y = yOffset - m_clientArea.h + LINE_HEIGHT * 2;
    LayoutScrollbars();
}

void SourceFileWindow::MakeCursorVisible()
{
    int yOffset = m_cursor.y * LINE_HEIGHT;
    if (yOffset < m_clientContentOffset.y)
        m_clientContentOffset.y = yOffset;
    if (yOffset > (m_clientContentOffset.y + m_clientArea.h - LINE_HEIGHT * 2))
        m_clientContentOffset.y = yOffset - m_clientArea.h + LINE_HEIGHT * 2;

    if (m_clientContentSize.x < m_clientArea.w)
        m_clientContentOffset.x = 0;
    else
    {
        Recti rect = CalcCursorArea();
        if (rect.x < m_clientContentOffset.x)
            m_clientContentOffset.x = rect.x;
        else if (rect.x > m_clientContentOffset.x + m_clientArea.w)
            m_clientContentOffset.x = rect.x - m_clientArea.w;
    }
    LayoutScrollbars();
}

void SourceFileWindow::MakeAddressVisible(int addr)
{
    auto dis = SourceFileManager::Instance().GetDisassembly(m_sourceFile);
    if (dis)
    {
        for (int row = 0; row < m_sourceFile->m_lines.size(); row++)
        {
            auto ln = m_sourceFile->m_lines[row];
            if (ln->m_assembledLine >= 0 && ln->m_assembledLine < dis->m_lines.size())
            {
                auto& disline = dis->m_lines[ln->m_assembledLine];
                if ((u32)addr >= disline.m_addressStart && (u32)addr < disline.m_addressStart + disline.m_addressLength)
                {
                    MakeRowVisible(row);
                    return;
                }
            }

        }
    }
}


class SFC_DeleteLine : public SourceFileCmd
{
public:
    SFC_DeleteLine(int lineNmbr, const std::string& chars, const Vec2i& oldCursor, const Vec2i& newCursor) : m_lineNmbr(lineNmbr), m_oldChars(chars), SourceFileCmd(oldCursor, newCursor) {}

    void DoExecute(SourceFile* file) override
    {
        file->m_lines.erase(file->m_lines.begin() + m_lineNmbr, file->m_lines.begin() + m_lineNmbr + 1);
    }

    void DoRevert(SourceFile* file) override
    {
        auto line = new SourceLine;
        line->m_uniqueID = file->m_uniqueLineID++;
        line->m_chars = m_oldChars;
        file->m_lines.insert(file->m_lines.begin() + m_lineNmbr, line);
    }

    std::string Desc() 
    {
        std::string desc = std::format("Delete Line {}: {}", m_lineNmbr, m_oldChars);
        std::ranges::replace(desc, '\t', ' ');
        return desc;
    }

    int m_lineNmbr = -1;
    std::string m_oldChars;
};

class SFC_ReplaceLine : public SourceFileCmd
{
public:
    SFC_ReplaceLine(int lineNmbr, const std::string& oldChars, const std::string& chars, const Vec2i& oldCursor, const Vec2i& newCursor) : m_lineNmbr(lineNmbr), m_chars(chars), m_oldChars(oldChars), SourceFileCmd(oldCursor, newCursor) {}

    void DoExecute(SourceFile* file) override
    {
        auto line = file->m_lines[m_lineNmbr];
        line->m_chars = m_chars;
        line->m_fragmentsDirty = true;
    }

    void DoRevert(SourceFile* file) override
    {
        auto line = file->m_lines[m_lineNmbr];
        line->m_chars = m_oldChars;
        line->m_fragmentsDirty = true;
    }

    std::string Desc()
    {
        std::string desc = std::format("{}: {}", m_lineNmbr, m_chars);
        std::ranges::replace(desc, '\t', ' ');
        return desc;
    }

    int m_lineNmbr = -1;
    std::string m_oldChars;
    std::string m_chars;
};

class SFC_InsertLine : public SourceFileCmd
{
public:
    SFC_InsertLine(int lineNmbr, const std::string& chars, const Vec2i& oldCursor, const Vec2i& newCursor) : m_lineNmbr(lineNmbr), m_chars(chars), SourceFileCmd(oldCursor, newCursor) {}

    void DoExecute(SourceFile* file) override
    {
        auto line = new SourceLine;
        line->m_uniqueID = file->m_uniqueLineID++;
        line->m_chars = m_chars;
        file->m_lines.insert(file->m_lines.begin() + m_lineNmbr, line);
    }

    void DoRevert(SourceFile* file) override
    {
        file->m_lines.erase(file->m_lines.begin() + m_lineNmbr);
    }

    std::string Desc()
    {
        std::string desc = std::format("Insert Line {}: {}", m_lineNmbr, m_chars);
        std::ranges::replace(desc, '\t', ' ');
        return desc;
    }

    int m_lineNmbr = -1;
    std::string m_chars;
};


void SourceFileWindow::DeleteCharBeforeCursor()
{
    Assert(!m_sourceFile->m_lines.empty(), "Source File Can't be Empty!");

    if (m_cursor.x == 0)
    {
        // append line to end of previous line
        std::string removedLine = m_sourceFile->m_lines[m_cursor.y]->m_chars;
        std::string oldLine = m_sourceFile->m_lines[m_cursor.y - 1]->m_chars;
        std::string newLine = oldLine;
        newLine.insert(newLine.end(), removedLine.begin(), removedLine.end());

        Vec2i oldCursor = m_cursor;
        Vec2i newCursor = { (int)oldLine.size(), m_cursor.y - 1};

        auto sfcDelete = new SFC_DeleteLine(m_cursor.y, removedLine, oldCursor, newCursor);
        auto sfcReplace = new SFC_ReplaceLine(m_cursor.y - 1, oldLine, newLine, oldCursor, newCursor);
        auto sfcGroup = new SFC_Group("Collapse Lines", oldCursor, newCursor);
        sfcGroup->m_cmds.push_back(sfcDelete);
        sfcGroup->m_cmds.push_back(sfcReplace);

        m_sourceFile->m_cmdBuffer->PushAndExecute(m_sourceFile, sfcGroup);

        m_cursor = newCursor;
        m_trackedColumn = m_cursor.x;
        m_clientContentSize.y = (int)m_sourceFile->m_lines.size() * LINE_HEIGHT;
    }
    else
    {
        auto oldLine = m_sourceFile->m_lines[m_cursor.y]->m_chars;
        auto newLine = oldLine;
        newLine.erase(newLine.begin() + m_cursor.x-1, newLine.begin() + m_cursor.x);

        Vec2i oldCursor = m_cursor;
        Vec2i newCursor = { m_cursor.x-1, m_cursor.y };

        auto sfcReplace = new SFC_ReplaceLine(m_cursor.y, oldLine, newLine, oldCursor, newCursor);
        m_sourceFile->m_cmdBuffer->PushAndExecute(m_sourceFile, sfcReplace);

        m_cursor = newCursor;
        m_trackedColumn = m_cursor.x;
        m_clientContentSize.y = (int)m_sourceFile->m_lines.size() * LINE_HEIGHT;
    }
}

void SourceFileWindow::DeleteCharAfterCursor()
{
    Assert(!m_sourceFile->m_lines.empty(), "Source File Can't be Empty!");

    if ((m_cursor.y == (m_sourceFile->m_lines.size() - 1)) && (m_cursor.x == m_sourceFile->m_lines[m_cursor.y]->m_chars.size()))
        return;

    Vec2i oldCursor = m_cursor;
    Vec2i newCursor = m_cursor;

    if (m_cursor.x == m_sourceFile->m_lines[m_cursor.y]->m_chars.size())
    {
        // append next line to end of this line, and remove the next line
        auto line = m_sourceFile->m_lines[m_cursor.y];
        auto nextLine = m_sourceFile->m_lines[m_cursor.y+1];

        std::string oldText = line->m_chars;
        std::string newText = oldText;
        newText.insert(newText.end(), nextLine->m_chars.begin(), nextLine->m_chars.end());

        auto sfcReplace = new SFC_ReplaceLine(m_cursor.y, oldText, newText, oldCursor, newCursor);
        auto sfcDelete = new SFC_DeleteLine(m_cursor.y + 1, nextLine->m_chars, oldCursor, newCursor);
        auto sfcGroup = new SFC_Group("Collapse Lines", oldCursor, newCursor);
        sfcGroup->m_cmds.push_back(sfcReplace);
        sfcGroup->m_cmds.push_back(sfcDelete);
        m_sourceFile->m_cmdBuffer->PushAndExecute(m_sourceFile, sfcGroup);
        m_clientContentSize.y = (int)m_sourceFile->m_lines.size() * LINE_HEIGHT;
    }
    else
    {
        auto oldLine = m_sourceFile->m_lines[m_cursor.y]->m_chars;
        auto newLine = oldLine;
        newLine.erase(newLine.begin() + m_cursor.x, newLine.begin() + m_cursor.x + 1);
        auto sfcReplace = new SFC_ReplaceLine(m_cursor.y, oldLine, newLine, oldCursor, newCursor);
        m_sourceFile->m_cmdBuffer->PushAndExecute(m_sourceFile, sfcReplace);
        m_clientContentSize.y = (int)m_sourceFile->m_lines.size() * LINE_HEIGHT;
    }
}

void SourceFileWindow::InsertNewLineAtCursor()
{
    Assert(!m_sourceFile->m_lines.empty(), "Source File Can't be Empty!");

    if (m_cursor.x == 0)
    {
        Vec2i oldCursor = m_cursor;
        Vec2i newCursor{ m_cursor.x, m_cursor.y+1 };

        auto sfcInsertLine = new SFC_InsertLine(m_cursor.y, "", oldCursor, newCursor);
        m_sourceFile->m_cmdBuffer->PushAndExecute(m_sourceFile, sfcInsertLine);
        m_cursor = newCursor;
        m_clientContentSize.y = (int)m_sourceFile->m_lines.size() * LINE_HEIGHT;
    }
    else if (m_cursor.x == m_sourceFile->m_lines[m_cursor.y]->m_chars.size())
    {
        Vec2i oldCursor = m_cursor;
        Vec2i newCursor{ 0, m_cursor.y + 1 };

        auto sfcInsertLine = new SFC_InsertLine(m_cursor.y+1, "", oldCursor, newCursor);
        m_sourceFile->m_cmdBuffer->PushAndExecute(m_sourceFile, sfcInsertLine);
        m_cursor = newCursor;
        m_trackedColumn = m_cursor.x;
        m_clientContentSize.y = (int)m_sourceFile->m_lines.size() * LINE_HEIGHT;
    }
    else
    {
        Vec2i oldCursor = m_cursor;
        Vec2i newCursor{ 0, m_cursor.y + 1 };

        // split current line and insert second part as a new line
        std::string &lineStr = m_sourceFile->m_lines[m_cursor.y]->m_chars;
        std::string firstPart = lineStr.substr(0, m_cursor.x);
        std::string secondPart = lineStr.substr(m_cursor.x);
        auto sfcReplaceLine = new SFC_ReplaceLine(m_cursor.y, lineStr, firstPart, oldCursor, newCursor);
        auto sfcInsertLine = new SFC_InsertLine(m_cursor.y + 1, secondPart, oldCursor, newCursor);
        auto sfcGroup = new SFC_Group("Split Line", oldCursor, newCursor);
        sfcGroup->m_cmds.push_back(sfcReplaceLine);
        sfcGroup->m_cmds.push_back(sfcInsertLine);
        m_sourceFile->m_cmdBuffer->PushAndExecute(m_sourceFile, sfcGroup);
        m_cursor = newCursor;
        m_trackedColumn = m_cursor.x;
        m_clientContentSize.y = (int)m_sourceFile->m_lines.size() * LINE_HEIGHT;
    }
}

void SourceFileWindow::SetBreakpoint(int lineID, int breakpointID)
{
    for (auto line : m_sourceFile->m_lines)
    {
        if (line->m_uniqueID == lineID)
        {
            // clear out the old breakpoint if somehow its set and doesn't match
            if (line->m_breakpointID && line->m_breakpointID != breakpointID)
            {
                gViceBridge->ClearBreakpoint(line->m_breakpointID);
            }

            line->m_breakpointID = breakpointID;
            return;
        }
    }

    // line doesn't exist anymore so just destroy this breakpoint
    gViceBridge->ClearBreakpoint(breakpointID);
}

void SourceFileWindow::ToggleBreakpoint(int line)
{
    if (line >= 0 && line < m_sourceFile->m_lines.size())
    {
        auto ln = m_sourceFile->m_lines[line];
        if (ln->m_breakpointID == 0)
        {
            auto dis = SourceFileManager::Instance().GetDisassembly(m_sourceFile);
            if (dis && ln->m_assembledLine >= 0 && ln->m_assembledLine < dis->m_lines.size())
            {
                auto& disline = dis->m_lines[ln->m_assembledLine];

                auto cmd = new VBC_SetBreakpoint;
                cmd->m_fileID = m_sourceFile->m_fileID;
                cmd->m_lineID = ln->m_uniqueID;
                cmd->m_addr = disline.m_addressStart;
                gViceBridge->SendMad2Vice(cmd);
            }
        }
        else
        {
            gViceBridge->ClearBreakpoint(ln->m_breakpointID);
            ln->m_breakpointID = 0;
        }
    }
}

void SourceFileWindow::InsertTextAtCursor(const char *text)
{
    std::string stext(text);
    std::string& lineStr = m_sourceFile->m_lines[m_cursor.y]->m_chars;
    std::string outtext = lineStr;

    Vec2i oldCursor = m_cursor;
    Vec2i newCursor{ m_cursor.x + (int)stext.size(), m_cursor.y };

    outtext.insert(outtext.begin() + m_cursor.x, stext.begin(), stext.end());
    auto sfcReplaceLine = new SFC_ReplaceLine(m_cursor.y, lineStr, outtext, oldCursor, newCursor);
    m_sourceFile->m_cmdBuffer->PushAndExecute(m_sourceFile, sfcReplaceLine);
    m_cursor = newCursor;
    m_trackedColumn = m_cursor.x;
    m_clientContentSize.y = (int)m_sourceFile->m_lines.size() * LINE_HEIGHT;
}

void SourceFileWindow::DeleteSelected()
{
    if (m_marked && (m_cursor.x != m_markStart.x || m_cursor.y != m_markStart.y))
    {
        Vec2i topPos = (m_cursor.y < m_markStart.y || (m_cursor.y == m_markStart.y && m_cursor.x < m_markStart.x)) ? m_cursor : m_markStart;
        Vec2i bottomPos = (m_cursor.y < m_markStart.y || (m_cursor.y == m_markStart.y && m_cursor.x < m_markStart.x)) ? m_markStart : m_cursor;

        Vec2i oldCursor = m_cursor;
        Vec2i newCursor = topPos;

        auto sfcGroup = new SFC_Group("Delete Marked", oldCursor, newCursor);

        if (topPos.y == bottomPos.y)
        {
            // single line cut
            auto& lineStr = m_sourceFile->m_lines[topPos.y]->m_chars;
            std::string newLineStr = lineStr;
            newLineStr.erase(newLineStr.begin() + topPos.x, newLineStr.begin() + bottomPos.x);
            auto sfcReplace = new SFC_ReplaceLine(topPos.y, lineStr, newLineStr, m_cursor, m_cursor);
            sfcGroup->m_cmds.push_back(sfcReplace);
        }
        else
        {
            // the start & end lines get combined, we delete the rest
            std::string& topLine = m_sourceFile->m_lines[topPos.y]->m_chars;
            std::string& bottomLine = m_sourceFile->m_lines[bottomPos.y]->m_chars;
            std::string joinedLine = topLine.substr(0, topPos.x);
            std::string cutBottomLine = bottomLine.substr(bottomPos.x);
            joinedLine.insert(joinedLine.end(), cutBottomLine.begin(), cutBottomLine.end());
            auto sfcReplace = new SFC_ReplaceLine(topPos.y, topLine, joinedLine, m_cursor, m_cursor);
            sfcGroup->m_cmds.push_back(sfcReplace);

            // now delete all the other lines
            for (int i = bottomPos.y; i > topPos.y; i--)
            {
                auto& lineStr = m_sourceFile->m_lines[i]->m_chars;
                auto sfcDeleteLine = new SFC_DeleteLine(i, lineStr, m_cursor, m_cursor);
                sfcGroup->m_cmds.push_back(sfcDeleteLine);
            }
        }

        m_sourceFile->m_cmdBuffer->PushAndExecute(m_sourceFile, sfcGroup);

        m_cursor = newCursor;
        m_trackedColumn = m_cursor.x;
        m_clientContentSize.y = (int)m_sourceFile->m_lines.size() * LINE_HEIGHT;
    }
    m_marked = false;
    m_marking = false;
    WindowManager::Instance().GetActiveWindowTree()->m_dirty = true;
}

void SourceFileWindow::CopySelected()
{
    if (m_marked && (m_cursor.x != m_markStart.x || m_cursor.y != m_markStart.y))
    {
        Vec2i topPos = (m_cursor.y < m_markStart.y || (m_cursor.y == m_markStart.y && m_cursor.x < m_markStart.x)) ? m_cursor : m_markStart;
        Vec2i bottomPos = (m_cursor.y < m_markStart.y || (m_cursor.y == m_markStart.y && m_cursor.x < m_markStart.x)) ? m_markStart : m_cursor;

        std::string buffer;
        if (topPos.y == bottomPos.y)
        {
            std::string& lineStr = m_sourceFile->m_lines[topPos.y]->m_chars;
            buffer = lineStr.substr(topPos.x, bottomPos.x - topPos.x);
        }
        else
        {
            for (int i = topPos.y; i <= bottomPos.y; i++)
            {
                std::string& lineStr = m_sourceFile->m_lines[i]->m_chars;
                if (i == topPos.y)
                {
                    buffer = lineStr.substr(topPos.x);
                    buffer.push_back('\n');
                }
                else if (i == bottomPos.y)
                {
                    std::string appendStr = lineStr.substr(0, bottomPos.x);
                    buffer.insert(buffer.end(), appendStr.begin(), appendStr.end());
                }
                else
                {
                    buffer.insert(buffer.end(), lineStr.begin(), lineStr.end());
                    buffer.push_back('\n');
                }
            }
        }
        SDL_SetClipboardText(buffer.c_str());
    }
}

void SourceFileWindow::PasteSelected()
{
    auto text = SDL_GetClipboardText();
    std::vector<std::string> paste;
    std::string buffer;
    for (char* ch = text; *ch; ch++)
    {
        if (*ch == '\r')
            continue;
        if (*ch == '\n')
        {
            paste.push_back(std::move(buffer));
        }
        else
        {
            buffer.push_back(*ch);
        }
    }
    paste.push_back(std::move(buffer));

    if (paste.size() == 1)
    {
        auto& lineStr = m_sourceFile->m_lines[m_cursor.y]->m_chars;
        std::string newLineStr = lineStr;
        newLineStr.insert(newLineStr.begin() + m_cursor.x, paste[0].begin(), paste[0].end());

        Vec2i oldCursor = m_cursor;
        Vec2i newCursor = { m_cursor.x + (int)paste[0].size(), m_cursor.y };

        auto sfcGroup = new SFC_Group("Paste Clipboard", oldCursor, newCursor);
        auto sfcReplace = new SFC_ReplaceLine(m_cursor.y, lineStr, newLineStr, oldCursor, newCursor);
        sfcGroup->m_cmds.push_back(sfcReplace);
        m_sourceFile->m_cmdBuffer->PushAndExecute(m_sourceFile, sfcGroup);

        m_cursor = newCursor;
        m_trackedColumn = m_cursor.x;
        m_clientContentSize.y = (int)m_sourceFile->m_lines.size() * LINE_HEIGHT;
    }
    else
    {
        auto& firstLine = m_sourceFile->m_lines[m_cursor.y]->m_chars;
        std::string firstLineStart = firstLine.substr(0, m_cursor.x);
        std::string firstLineEnd = firstLine.substr(m_cursor.x);
        int endx = (int)paste.back().size();
        paste[0].insert(paste[0].begin(), firstLineStart.begin(), firstLineStart.end());
        paste.back().insert(paste.back().end(), firstLineEnd.begin(), firstLineEnd.end());

        Vec2i oldCursor = m_cursor;
        Vec2i newCursor = { endx, m_cursor.y + (int)paste.size() - 1 };

        auto sfcGroup = new SFC_Group("Paste Clipboard", oldCursor, newCursor);
        auto sfcReplace = new SFC_ReplaceLine(m_cursor.y, firstLine, paste[0], oldCursor, newCursor);
        sfcGroup->m_cmds.push_back(sfcReplace);

        for (int i = 1; i < paste.size(); i++)
        {
            auto sfcInsertLine = new SFC_InsertLine(m_cursor.y + i, paste[i], oldCursor, newCursor);
            sfcGroup->m_cmds.push_back(sfcInsertLine);
        }
        m_sourceFile->m_cmdBuffer->PushAndExecute(m_sourceFile, sfcGroup);

        m_cursor = newCursor;
        m_trackedColumn = m_cursor.x;
        m_clientContentSize.y = (int)m_sourceFile->m_lines.size() * LINE_HEIGHT;
    }

    m_clientContentSize.y = (int)m_sourceFile->m_lines.size() * LINE_HEIGHT;
    m_marked = false;
    m_marking = false;
    WindowManager::Instance().GetActiveWindowTree()->m_dirty = true;
}

void SourceFileWindow::SaveTokens(std::vector<std::string>& layoutTokens)
{
    layoutTokens.push_back("SOURCE");
    layoutTokens.push_back(m_sourceFile->m_path);
}

bool SourceFileWindow::CreateFromLayoutTokens(WindowLayout* layout, const std::vector<std::string>& layoutTokens, size_t& idx)
{
    if (layoutTokens[idx] != "SOURCE")
        return false;

    auto& sfm = SourceFileManager::Instance();

    idx++;
    std::string path = layoutTokens[idx++];
    std::vector<std::string> fileList;
    fileList.push_back(path);
    sfm.RequestLoadFiles(fileList);
    sfm.LoadRequestedFiles(false);
    auto file = sfm.FindFile(path);
    Assert(file != nullptr, "Unable to load {}", path);
    auto win = new SourceFileWindow(file);
    layout->m_tabs.push_back(win);
    return true;
}

void SourceFileWindow::MessageChild(WindowLayout *layout, struct WindowMessageStruct& msg)
{
    switch (msg.m_type)
    {
        case WindowMessage::Window_SetCursor:
            {
                m_cursor.x = msg.m_x;
                m_cursor.y = msg.m_y;
                m_marking = false;
                ClampCursor();
                MakeCursorVisible();
            }
            break;

        case WindowMessage::Window_BreakpointHit:
        {
            if (m_sourceFile == msg.m_sourceFile)
            {
                auto& viceState = gViceBridge->GetViceState();
                MakeAddressVisible(viceState.m_pc);
                if (m_sourceFile->m_stepOverAddr == viceState.m_pc)
                {
                    gViceBridge->ClearBreakpoint(m_sourceFile->m_stepOverBreakpointID);
                    m_sourceFile->m_stepOverBreakpointID = 0;
                }
                else
                {
                    m_breakpointFlash = 1.0f;
                }
                m_sourceFile->m_stepOverAddr = -1;
            }
        }
        break;

        case WindowMessage::File_Deleted:
            if (m_sourceFile == msg.m_sourceFile)
            {
                WindowManager::Instance().QueueRemoveWindow(this);
            }
            break;
        case WindowMessage::File_Renamed:
            if (m_sourceFile == msg.m_sourceFile)
            {
                std::filesystem::path p = m_sourceFile->m_path;
                m_name = p.filename().string();
            }
            break;
        case WindowMessage::Query_FileCount:
            if (m_sourceFile == msg.m_sourceFile)
            {
                msg.m_response++;
            }
            break;
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

void SourceFileWindow::ReplaceLines(const std::vector<SearchResult>& lines, const std::string& text)
{
    if (lines.empty())
        return;

    int lastLine = lines.back().m_line;
    if (lastLine >= m_sourceFile->m_lines.size())
        return;

    Vec2i oldCursor = m_cursor;
    Vec2i newCursor = { (int)m_sourceFile->m_lines[lastLine]->m_chars.size(), lastLine};

    auto sfcGroup = new SFC_Group("Search and Replace", oldCursor, newCursor);
    for (auto line : lines)
    {
        std::string oldChars = m_sourceFile->m_lines[line.m_line]->m_chars;
        std::string newChars = oldChars;
        newChars.replace(line.m_startChar, line.m_length, text);
        auto sfcReplace = new SFC_ReplaceLine(line.m_line, oldChars, newChars, oldCursor, newCursor);
        sfcGroup->m_cmds.push_back(sfcReplace);
    }
    m_sourceFile->m_cmdBuffer->PushAndExecute(m_sourceFile, sfcGroup);

    m_cursor = newCursor;
    m_trackedColumn = m_cursor.x;
    m_clientContentSize.y = (int)m_sourceFile->m_lines.size() * LINE_HEIGHT;
}
