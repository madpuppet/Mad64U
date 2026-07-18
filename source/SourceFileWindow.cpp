#include "common.h"
#include "SourceFileWindow.h"
#include "SourceFileManager.h"
#include "SourceFile.h"
#include "Application.h"
#include "SourceFileCmdBuffer.h"
#include "LogManager.h"
#include <filesystem>

#define TAB_SIZE 4

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
    auto& tp = Application::Instance().GetThemeProperties();
    auto window = WindowManager::Instance().GetActiveWindowBase();
    if (window == this)
        tp.SetRenderDrawColor(renderer, ThemeColor::SourceBackgroundSelected);
    else
        tp.SetRenderDrawColor(renderer, ThemeColor::SourceBackground);

    SDL_FRect body = m_clientArea.AsSDLFRect();
    SDL_RenderFillRect(renderer, &body);

    auto& fr = FontRenderer::Instance();

    int firstLine = Max(m_clientContentOffset.y / LINE_HEIGHT, 0);
    int lastLine = Min(firstLine + m_clientArea.h / LINE_HEIGHT, (int)m_sourceFile->m_lines.size());
    int x = 0;
    int y = firstLine * LINE_HEIGHT;
    int xBase = m_clientArea.x - m_clientContentOffset.x + BORDER_MARGIN;
    int yBase = m_clientArea.y - m_clientContentOffset.y + BORDER_MARGIN;

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
            BuildFragments(renderer, line);
        for (auto& fragment : line->m_fragments)
        {
            auto& col = tp.m_colors[(int)ThemeColor::TextGeneral + (int)fragment.m_fragType];
            fr.RenderText(renderer, fragment.m_chars, col, xBase + fragment.m_area.x, yBase + y, FontType::Text);
            maxWidth = Max(fragment.m_area.x + fragment.m_area.w, maxWidth);
        }
        y += LINE_HEIGHT;
    }
    m_clientContentSize.x = maxWidth + 32;

    // draw cursor
    SDL_FRect cursorRect = CalcCursorArea().AsSDLFRect();
    cursorRect.x += (float)xBase;
    cursorRect.y += (float)yBase;
    SDL_Color col = tp.m_colors[(int)ThemeColor::Cursor];
    col.a = (int)(cosf(m_animTime * 2.0f * SDL_PI_F) * 127) + 128;
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, col.a);
    SDL_RenderFillRect(renderer, &cursorRect);
}

const char* operators[] = {
    "lda", "sta", 0
};

void SourceFileWindow::BuildFragments(SDL_Renderer* renderer, SourceLine* line)
{
    int x = 0;
    int y = 0;
    SourceType sourceType = m_sourceFile->m_sourceType;
    auto& sm = SourceFileManager::Instance();
    line->m_fragmentsDirty = false;
    auto& fr = FontRenderer::Instance();
    char frag[1024];
    line->m_fragments.clear();
    int i = 0;
    int charCount = (int)line->m_chars.size();
    FragmentType ft = FragmentType::General;
    int chIdx = 0;
    while (i < charCount)
    {
        // skip white space
        while (i < charCount)
        {
            u32 ch = (u32)line->m_chars[i];     //TODO: UTF8 process
            if (ch == '\t')
            {
                chIdx = ((chIdx / TAB_SIZE) + 1) * TAB_SIZE;
                i++;
            }
            else if (ch == ' ')
            {
                chIdx++;
                i++;
            }
            else break;
        }

        // gather a fragment
        int c = 0;
        int wordChIdx = 0;
        while (i < charCount)
        {
            u32 ch = (u32)line->m_chars[i];     //TODO: UTF8 process
            if (ch == '\t' || ch == ' ')
                break;
            if (ch == ';' && c > 0)
                break;
            frag[c++] = (u8)ch;
            i++;
            wordChIdx++;
        }
        frag[c++] = 0;

        if (frag[0] == ';')
            ft = FragmentType::Comment;

        if (ft != FragmentType::Comment)
        {
            ft = sm.IsKeyword(sourceType, frag) ? FragmentType::Operator : FragmentType::General;
        }

        SourceLineRenderFragment fragment;
        fragment.m_fragType = ft;
        fragment.m_chars = frag;
        x = chIdx * m_charWidth;
        fr.CalcTextArea(renderer, fragment.m_chars, Vec2i(x, y), FontType::Text, fragment.m_area);
        x = fragment.m_area.x + fragment.m_area.w;
        line->m_fragments.emplace_back(fragment);
        chIdx += wordChIdx;
    }
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
    m_animTime += WINDOW_TICK_MS * (1.0f / 1000.0f);
    if (m_animTime > 1.0f)
        m_animTime -= 1.0f;
    return true;
}

void SourceFileWindow::CalcXYFromClientPos(int x, int y, int& col, int& row)
{
    auto &fr = FontRenderer::Instance();
    int xBase = m_clientArea.x - m_clientContentOffset.x + BORDER_MARGIN;
    int yBase = m_clientArea.y - m_clientContentOffset.y + BORDER_MARGIN;
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
                if (!m_shiftDown)
                {
                    ClearMarking();
                }
                StartMarking();
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
                case SDLK_S:
                    if (e->key.mod & SDL_KMOD_CTRL)
                    {
                        SourceFileManager::Instance().SaveFile(m_sourceFile);
                    }
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
    return m_sourceFile->m_cmdBuffer->m_cmdIndex > 0;
}

void SourceFileWindow::MoveCursorEndOfLine()
{
    Assert(!m_sourceFile->m_lines.empty(), "Source File Can't be Empty!");

    m_cursor.x = (int)m_sourceFile->m_lines[m_cursor.y]->m_chars.size();
    m_trackedColumn = m_cursor.x;
    m_animTime = 0.0f;
    MakeCursorVisible();
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
        auto sfcReplace = new SFC_ReplaceLine(m_cursor.y - 1, newLine, oldLine, oldCursor, newCursor);
        auto sfcGroup = new SFC_Group("Collapse Lines", oldCursor, newCursor);
        sfcGroup->m_cmds.push_back(sfcDelete);
        sfcGroup->m_cmds.push_back(sfcReplace);

        m_sourceFile->m_cmdBuffer->PushAndExecute(m_sourceFile, sfcGroup);

        m_cursor = newCursor;
        m_trackedColumn = m_cursor.x;
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
    }
    else
    {
        auto oldLine = m_sourceFile->m_lines[m_cursor.y]->m_chars;
        auto newLine = oldLine;
        newLine.erase(newLine.begin() + m_cursor.x, newLine.begin() + m_cursor.x + 1);
        auto sfcReplace = new SFC_ReplaceLine(m_cursor.y, oldLine, newLine, oldCursor, newCursor);
        m_sourceFile->m_cmdBuffer->PushAndExecute(m_sourceFile, sfcReplace);
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
    }
    else if (m_cursor.x == m_sourceFile->m_lines[m_cursor.y]->m_chars.size())
    {
        Vec2i oldCursor = m_cursor;
        Vec2i newCursor{ 0, m_cursor.y + 1 };

        auto sfcInsertLine = new SFC_InsertLine(m_cursor.y+1, "", oldCursor, newCursor);
        m_sourceFile->m_cmdBuffer->PushAndExecute(m_sourceFile, sfcInsertLine);
        m_cursor = newCursor;
        m_trackedColumn = m_cursor.x;
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
    }

    m_marked = false;
    m_marking = false;
    WindowManager::Instance().GetActiveWindowTree()->m_dirty = true;
}

void SourceFileWindow::Compile()
{
    SourceFileManager::Instance().SaveAll();

    auto compile = [file = m_sourceFile]()
        {
            LogManager::Instance().Clear(LogGroup::Build);

            std::filesystem::path workingDir = std::filesystem::path(file->m_path).parent_path() / "out";
            std::error_code ec;
            if (!std::filesystem::create_directories(workingDir, ec) && ec)
            {
                Log(LogGroup::Build,
                    "Failed to create directory '{}': {}\n",
                    workingDir.string(),
                    ec.message());
                return;
            }

            std::string wd = workingDir.string();
            const char* args[] =
            {
                "java", "-jar", "kickass\\kickass.jar", file->m_path.c_str(), "-bytedump", "-debugdump", "-odir", wd.c_str(), nullptr
            };

            SDL_Process* process = SDL_CreateProcess(args, true);
            if (!process)
            {
                Log(LogGroup::Build, "Failed to start process: {}\n", SDL_GetError());
            }

            size_t outputSize = 0;
            int exitCode = -1;
            void* outputData = SDL_ReadProcess(process, &outputSize, &exitCode);
            if (!outputData)
            {
                Log(LogGroup::Build, "SDL_ReadProcess failed: {}", SDL_GetError());
                SDL_DestroyProcess(process);
                return;
            }

            const std::string output( static_cast<const char*>(outputData), outputSize);
            SDL_free(outputData);
            SDL_DestroyProcess(process);

            std::istringstream stream(output);
            std::string line;
            while (std::getline(stream, line))
            {
                while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
                    line.pop_back();

                Log(LogGroup::Build, line);
            }
        };

    std::thread(compile).detach();
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

void SourceFileWindow::Message(struct WindowMessageStruct& msg)
{
    switch (msg.m_type)
    {
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
        case WindowMessage::File_Count:
            if (m_sourceFile == msg.m_sourceFile)
            {
                msg.m_count++;
            }
            break;
    }
}


