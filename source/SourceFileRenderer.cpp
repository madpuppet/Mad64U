#include "common.h"
#include "SourceFileRenderer.h"
#include "SourceFileManager.h"
#include "SourceFile.h"
#include "Application.h"
#include <filesystem>

#define SOURCE_FILE_LINE_HEIGHT 16
#define BORDER_MARGIN 4
#define CHAR_WIDTH 12
#define TAB_SIZE 4

SourceFileRenderer::SourceFileRenderer(SourceFile* file) : m_sourceFile(file)
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
        m_clientContentSize.y = (int)file->m_lines.size() * SOURCE_FILE_LINE_HEIGHT;
    }
}

Recti SourceFileRenderer::CalcCursorArea()
{
    auto& fr = FontRenderer::Instance();
    Recti rect = Recti{ 0,2,4,SOURCE_FILE_LINE_HEIGHT };
    if (!m_sourceFile->m_lines.empty())
    {
        auto line = m_sourceFile->m_lines[m_cursor.y];
        int cmax = Min(m_cursor.x, (int)line->m_chars.size());
        int x = 0;
        for (int i = 0; i < cmax; i++)
        {
            u32 ch = line->m_chars[i];
            if (ch == (u32)'\t')
                x += CHAR_WIDTH * TAB_SIZE;
            else
                x += fr.GetCharactorWidth(FontType::Text, ch);
        }
        rect.x = x;
        rect.y = m_cursor.y * SOURCE_FILE_LINE_HEIGHT + 2;
        if (m_overwrite)
        {
            if (cmax < line->m_chars.size())
            {
                u32 ch = line->m_chars[cmax];
                if (ch == (u32)'\t')
                    rect.w = CHAR_WIDTH * TAB_SIZE;
                else
                    rect.w = fr.GetCharactorWidth(FontType::Text, ch);
            }
        }
        rect.h = SOURCE_FILE_LINE_HEIGHT;
    }
    return rect;
}

void SourceFileRenderer::Paint(SDL_Renderer* renderer, const Recti& dirtyArea)
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

    int firstLine = m_clientContentOffset.y / SOURCE_FILE_LINE_HEIGHT;
    int lastLine = Min(firstLine + m_clientArea.h / SOURCE_FILE_LINE_HEIGHT, (int)m_sourceFile->m_lines.size());
    int x = 0;
    int y = firstLine * SOURCE_FILE_LINE_HEIGHT;
    int xBase = m_clientArea.x - m_clientContentOffset.x + BORDER_MARGIN;
    int yBase = m_clientArea.y - m_clientContentOffset.y + BORDER_MARGIN;

    int maxWidth = 0;
    for (int i = firstLine; i < lastLine; i++)
    {
        auto line = m_sourceFile->m_lines[i];
        if (line->m_fragmentsDirty)
            BuildFragments(renderer, line, x, y);
        for (auto& fragment : line->m_fragments)
        {
            auto& col = tp.m_colors[(int)ThemeColor::TextGeneral + (int)fragment.m_fragType];
            fr.RenderText(renderer, fragment.m_chars, col, xBase + fragment.m_area.x, yBase + fragment.m_area.y, FontType::Text);
            maxWidth = Max(fragment.m_area.x + fragment.m_area.w, maxWidth);
        }
        y += SOURCE_FILE_LINE_HEIGHT;
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

void SourceFileRenderer::BuildFragments(SDL_Renderer* renderer, SourceLine* line, int x, int y)
{
    SourceType sourceType = m_sourceFile->m_sourceType;
    auto& sm = SourceFileManager::Instance();
    line->m_fragmentsDirty = false;
    auto& fr = FontRenderer::Instance();
    char frag[1024];
    line->m_fragments.clear();
    int i = 0;
    int charCount = (int)line->m_chars.size();
    FragmentType ft = FragmentType::General;
    while (i < charCount)
    {
        // skip white space
        while (i < charCount)
        {
            u32 ch = (u32)line->m_chars[i];     //TODO: UTF8 process
            if (ch == '\t')
            {
                x += CHAR_WIDTH * TAB_SIZE;
                i++;
            }
            else if (ch == ' ')
            {
                x += fr.GetCharactorWidth(FontType::Text, ch);
                i++;
            }
            else break;
        }

        // gather a fragment
        int c = 0;
        while (i < charCount)
        {
            u32 ch = (u32)line->m_chars[i];     //TODO: UTF8 process
            if (ch == '\t' || ch == ' ')
                break;
            if (ch == ';' && c > 0)
                break;
            frag[c++] = (u8)ch;
            i++;
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
        fr.CalcTextArea(renderer, fragment.m_chars, Vec2i(x, y), FontType::Text, fragment.m_area);
        x = fragment.m_area.x + fragment.m_area.w;
        line->m_fragments.emplace_back(fragment);
    }
}

void SourceFileRenderer::Close()
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

    if (m_sourceFile->m_modified)
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

bool SourceFileRenderer::Tick()
{
    m_animTime += WINDOW_TICK_MS * (1.0f / 1000.0f);
    if (m_animTime > 1.0f)
        m_animTime -= 1.0f;
    return true;
}

void SourceFileRenderer::CalcXYFromClientPos(int x, int y, int& col, int& row)
{
    auto &fr = FontRenderer::Instance();
    int xBase = m_clientArea.x - m_clientContentOffset.x + BORDER_MARGIN;
    int yBase = m_clientArea.y - m_clientContentOffset.y + BORDER_MARGIN;
    row = Clamp((y - yBase) / SOURCE_FILE_LINE_HEIGHT, 0, (int)m_sourceFile->m_lines.size() - 1);

    auto line = m_sourceFile->m_lines[row];
    int x1 = 0;
    int x2 = 0;
    x -= xBase;
    col = 0;
    for (auto ch : line->m_chars)
    {
        if (ch == (u32)'\t')
            x2 = x1 + CHAR_WIDTH * TAB_SIZE;
        else
            x2 = x1 + fr.GetCharactorWidth(FontType::Text, ch);
        int xc = (x1 + x2) / 2;
        if (x < xc)
            break;
        col++;
        if (x < x2)
            break;
        x1 = x2;
    }
}

bool SourceFileRenderer::HandleEvent(SDL_Event* e)
{
    switch (e->type)
    {
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        {
            int row, col;
            CalcXYFromClientPos((int)e->button.x, (int)e->button.y, col, row);
            MoveCursorXY(col, row);
            return true;
        }
        break;

        case SDL_EVENT_MOUSE_MOTION:
        {
        }
        break;

        case SDL_EVENT_MOUSE_BUTTON_UP:
        {
        }
        break;

        case SDL_EVENT_MOUSE_WHEEL:
        {
            int scroll = e->wheel.integer_y * SOURCE_FILE_LINE_HEIGHT * 4;
            m_clientContentOffset.y -= scroll;
            m_clientContentOffset.y = Clamp(m_clientContentOffset.y, 0, (int)(m_sourceFile->m_lines.size()+1) * SOURCE_FILE_LINE_HEIGHT - m_clientArea.h);
            if (m_clientContentSize.x < m_clientArea.w)
                m_clientContentOffset.x = 0;
            LayoutScrollbars();
            return true;
        }
        break;

        case SDL_EVENT_TEXT_INPUT:
            break;

        case SDL_EVENT_KEY_DOWN:
            switch (e->key.key)
            {
                case SDLK_LEFT:
                    MoveCursorLeft();
                    return true;
                case SDLK_RIGHT:
                    MoveCursorRight();
                    return true;
                case SDLK_UP:
                    MoveCursorUp();
                    return true;
                case SDLK_DOWN:
                    MoveCursorDown();
                    return true;
                case SDLK_PAGEUP:
                    MoveCursorPageUp();
                    return true;
                case SDLK_PAGEDOWN:
                    MoveCursorPageDown();
                    return true;
                case SDLK_HOME:
                    MoveCursorStart();
                    return true;
                case SDLK_END:
                    MoveCursorEnd();
                    return true;
            }
            break;

        case SDL_EVENT_KEY_UP:
            break;
    }
    return false;
}


void SourceFileRenderer::MoveCursorLeft()
{
    if (!m_sourceFile->m_lines.empty())
    {
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
}

void SourceFileRenderer::MoveCursorRight()
{
    if (!m_sourceFile->m_lines.empty())
    {
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
}

void SourceFileRenderer::MoveCursorUp()
{
    if (!m_sourceFile->m_lines.empty())
    {
        if (m_cursor.y > 0)
        {
            m_cursor.y--;
            m_cursor.x = Min(m_trackedColumn, (int) m_sourceFile->m_lines[m_cursor.y]->m_chars.size());
            m_animTime = 0.0f;
            MakeCursorVisible();
        }
    }
}

void SourceFileRenderer::MoveCursorDown()
{
    if (!m_sourceFile->m_lines.empty())
    {
        if (m_cursor.y < m_sourceFile->m_lines.size()-1)
        {
            m_cursor.y++;
            m_cursor.x = Min(m_trackedColumn, (int)m_sourceFile->m_lines[m_cursor.y]->m_chars.size());
            m_animTime = 0.0f;
            MakeCursorVisible();
        }
    }
}

void SourceFileRenderer::MoveCursorPageUp()
{
    if (!m_sourceFile->m_lines.empty())
    {
        if (m_cursor.y > 0)
        {
            m_cursor.y = Max(m_cursor.y - m_clientArea.h / SOURCE_FILE_LINE_HEIGHT, 0);
            m_cursor.x = Min(m_trackedColumn, (int)m_sourceFile->m_lines[m_cursor.y]->m_chars.size());
            m_animTime = 0.0f;
            MakeCursorVisible();
        }
    }
}

void SourceFileRenderer::MoveCursorXY(int x, int y)
{
    m_cursor.x = x;
    m_cursor.y = y;
    m_trackedColumn = m_cursor.x;
    m_animTime = 0.0f;
    MakeCursorVisible();
}

void SourceFileRenderer::MoveCursorPageDown()
{
    if (!m_sourceFile->m_lines.empty())
    {
        if (m_cursor.y < m_sourceFile->m_lines.size() - 1)
        {
            m_cursor.y = Min(m_cursor.y + m_clientArea.h / SOURCE_FILE_LINE_HEIGHT, (int)m_sourceFile->m_lines.size()-1);
            m_cursor.x = Min(m_trackedColumn, (int)m_sourceFile->m_lines[m_cursor.y]->m_chars.size());
            m_animTime = 0.0f;
            MakeCursorVisible();
        }
    }
}

void SourceFileRenderer::MoveCursorStart()
{
    if (!m_sourceFile->m_lines.empty())
    {
        m_cursor.x = 0;
        m_cursor.y = 0;
        m_trackedColumn = m_cursor.x;
        m_animTime = 0.0f;
        MakeCursorVisible();
    }
}

void SourceFileRenderer::MoveCursorEnd()
{
    if (!m_sourceFile->m_lines.empty())
    {
        m_cursor.y = (int)(m_sourceFile->m_lines.size()-1);
        m_cursor.x = (int)m_sourceFile->m_lines[m_cursor.y]->m_chars.size();
        m_trackedColumn = m_cursor.x;
        m_animTime = 0.0f;
        MakeCursorVisible();
    }
}

void SourceFileRenderer::MakeCursorVisible()
{
    int yOffset = m_cursor.y * SOURCE_FILE_LINE_HEIGHT;
    if (yOffset < m_clientContentOffset.y)
        m_clientContentOffset.y = yOffset;
    if (yOffset > (m_clientContentOffset.y + m_clientArea.h - SOURCE_FILE_LINE_HEIGHT * 2))
        m_clientContentOffset.y = yOffset - m_clientArea.h + SOURCE_FILE_LINE_HEIGHT * 2;

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


