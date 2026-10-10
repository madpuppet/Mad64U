#include "common.h"
#include "SearchWindow.h"
#include "SourceFileManager.h"
#include "Application.h"
#include "WindowManager.h"
#include "FontRenderer.h"
#include "IconRenderer.h"
#include "LogManager.h"
#include "SourceFileCmdBuffer.h"
#include "SourceFileWindow.h"
#include <filesystem>

void TextBox::SetInfo(const std::string title, const Recti& area)
{
    m_charWidth = FontRenderer::Instance().GetCharactorWidth(FontType::UI, ' ');
    m_stringPos = { area.x, area.y };
    m_title = title;
    m_area = { area.x + 100, area.y + 1, area.w - 100, area.h - 2 };
}
void TextBox::Focus(bool enable)
{
    m_focused = enable;
}

void TextBox::SetCursorPos(int x)
{
    int cx = (x - m_area.x - BORDER_MARGIN) / m_charWidth;
    m_cursor = Clamp(cx, 0, (int)m_text.size());
}

void TextBox::Paint(SDL_Renderer* renderer)
{
    auto& fr = FontRenderer::Instance();
    auto& tp = Application::Instance().GetThemeProperties();

    SDL_Color backColor, titleColor, textColor;
    backColor = m_focused ? tp.Col(ThemeColor::SearchTextBackSelected) : tp.Col(ThemeColor::SearchTextBack);
    titleColor = tp.Col(ThemeColor::SearchTitle);
    textColor = tp.Col(ThemeColor::TextGeneral);

    SDL_FRect backArea = m_area.AsSDLFRect();
    SDL_SetRenderDrawColor(renderer, backColor.r, backColor.g, backColor.b, backColor.a);
    SDL_RenderFillRect(renderer, &backArea);
    fr.RenderText(renderer, m_title, titleColor, m_stringPos.x, m_stringPos.y, FontType::UI);
    fr.RenderText(renderer, m_text, textColor, (int)(backArea.x + BORDER_MARGIN), (int)backArea.y, FontType::UI);

    // draw cusror
    if (m_focused)
    {
        int w = fr.GetCharactorWidth(FontType::UI, ' ');
        int cx = w * m_cursor;
        SDL_Color cursorCol = tp.Col(ThemeColor::Cursor);
        cursorCol.a = (int)(cosf(m_animTime * 2.0f * SDL_PI_F) * 127) + 128;
        SDL_SetRenderDrawColor(renderer, cursorCol.r, cursorCol.g, cursorCol.b, cursorCol.a);
        SDL_FRect cursorArea = { backArea.x + BORDER_MARGIN + cx - 1, backArea.y, 3, backArea.h };
        SDL_RenderFillRect(renderer, &cursorArea);
    }
}

void TextBox::Tick()
{
    m_animTime += WINDOW_TICK_MS * (1.0f / 1000.0f);
    if (m_animTime > 1.0f)
        m_animTime -= 1.0f;
}

bool TextBox::HandleEvent(SDL_Event* e)
{
    switch (e->type)
    {
        case SDL_EVENT_KEY_DOWN:
            switch (e->key.key)
            {
                case SDLK_LEFT:
                    MoveCursor(-1);
                    return true;
                case SDLK_RIGHT:
                    MoveCursor(1);
                    return true;
                case SDLK_HOME:
                    MoveCursorStart();
                    return true;
                case SDLK_END:
                    MoveCursorEnd();
                    return true;
                case SDLK_BACKSPACE:
                    if (m_cursor > 0)
                        m_text.erase(--m_cursor, 1);
                    return true;
                case SDLK_DELETE:
                    if (m_cursor < m_text.size())
                        m_text.erase(m_cursor, 1);
                    return true;
                case SDLK_V:
                    if (e->key.mod & SDL_KMOD_CTRL)
                    {
                        std::string paste;
                        auto text = SDL_GetClipboardText();
                        for (char* ch = text; *ch; ch++)
                        {
                            if (*ch == '\n' || *ch == '\t' || *ch == '\r')
                                break;

                            paste.push_back(*ch);
                        }
                        m_text.insert(m_cursor, paste);
                        m_cursor += (int)paste.size();
                    }
                    return true;
            }
            break;
        case SDL_EVENT_TEXT_INPUT:
            m_text.insert(m_cursor, e->text.text);
            m_cursor += (int)strlen(e->text.text);
            break;
    }
    return false;
}

void SearchWindow::SetSearchActive()
{
    m_searchBox.Focus(true);
}

SearchWindow::SearchWindow()
{
    m_name = "Search";
}

SearchWindow::~SearchWindow()
{
}

void SearchWindow::Paint(SDL_Renderer* renderer, const Recti& dirtyArea)
{
    int charWidth = FontRenderer::Instance().GetCharactorWidth(FontType::Text, ' ');

    if (WindowManager::Instance().GetActiveWindowBase() != this)
    {
        m_searchBox.Focus(false);
        m_replaceBox.Focus(false);
    }

    auto& sm = SourceFileManager::Instance();
    auto& fr = FontRenderer::Instance();
    auto& tp = Application::Instance().GetThemeProperties();
    auto& ir = IconRenderer::Instance();
    auto& wm = WindowManager::Instance();
    auto& highlight = wm.GetWindowHighlightQuery();

    // draw background
    auto window = WindowManager::Instance().GetActiveWindowBase();
    if (window == this)
        tp.SetRenderDrawColor(renderer, ThemeColor::SourceBackgroundSelected);
    else
        tp.SetRenderDrawColor(renderer, ThemeColor::SourceBackground);
    SDL_FRect body = m_clientArea.AsSDLFRect();
    SDL_RenderFillRect(renderer, &body);

    SDL_FRect sarArea = { (float)m_clientArea.x, (float)m_clientArea.y, (float)m_clientArea.w, (float)LINE_HEIGHT * 3 + BORDER_MARGIN };
    m_searchBox.SetInfo("Search", { m_clientArea.x + BORDER_MARGIN, m_clientArea.y + LINE_HEIGHT, m_clientArea.w - BORDER_MARGIN * 2 , LINE_HEIGHT });
    m_replaceBox.SetInfo("Replace", { m_clientArea.x + BORDER_MARGIN, m_clientArea.y + LINE_HEIGHT*2, m_clientArea.w - BORDER_MARGIN * 2, LINE_HEIGHT });

    tp.SetRenderDrawColor(renderer, ThemeColor::SearchTitleBack);
    SDL_RenderFillRect(renderer, &sarArea);

    m_searchBox.Paint(renderer);
    m_replaceBox.Paint(renderer);

    auto activeSource = SourceFileManager::Instance().GetActiveSourceFile();
    if (activeSource)
    {
        std::string filename = std::filesystem::path(activeSource->m_path).filename().string();
        std::string src = std::format("SOURCE: {}", filename);
        SDL_Color col = tp.Col(ThemeColor::TextOperator);
        fr.RenderText(renderer, src, col, m_clientArea.x + BORDER_MARGIN, m_clientArea.y, FontType::UI);
    }

    if (!m_searchFile || m_searchLines.empty())
        return;

    int firstLine = Max(m_clientContentOffset.y / LINE_HEIGHT, 0);
    int lastLine = Min(firstLine + m_clientArea.h / LINE_HEIGHT + 2, (int)m_searchLines.size());
    int xBase = m_clientArea.x - m_clientContentOffset.x + BORDER_MARGIN;
    int yBase = (int)(sarArea.y + sarArea.h) + BORDER_MARGIN - m_clientContentOffset.y;
    int x = xBase;
    int y = yBase + firstLine * LINE_HEIGHT;
    int w = 0;
    int maxWidth = 64;
    m_searchLineArea = { m_clientArea.x, (int)(sarArea.y + sarArea.h), m_clientArea.w, m_clientArea.h - (int)sarArea.h };

    SDL_Rect oldClipArea;
    SDL_GetRenderClipRect(renderer, &oldClipArea);
    SDL_Rect clipArea{ m_clientArea.x, (int)(sarArea.y + sarArea.h), m_clientArea.w, m_clientArea.h - (int)sarArea.h };
    SDL_SetRenderClipRect(renderer, &clipArea);

    for (int i = firstLine; i < lastLine; i++)
    {
        auto &foundResult = m_searchLines[i];
        if (foundResult.m_line >= m_searchFile->m_lines.size())
            break;

        auto line = m_searchFile->m_lines[foundResult.m_line];

        fr.RenderText(renderer, std::format("{:5d}", foundResult.m_line), tp.m_colors[(int)ThemeColor::TextString], xBase, y, FontType::Text);
        x = xBase + 60;

        tp.SetRenderDrawColor(renderer, ThemeColor::TextHighlight);
        int startCol = line->CharIndexToColumn(foundResult.m_startChar);
        int endCol = line->CharIndexToColumn(foundResult.m_startChar + foundResult.m_length);
        SDL_FRect area{ (float)(x + startCol * charWidth), (float)y, (float)((endCol - startCol) * charWidth), (float)LINE_HEIGHT };
        SDL_RenderFillRect(renderer, &area);

        if (line->m_fragmentsDirty)
            line->BuildFragments(renderer, m_searchFile->m_sourceType);
        for (auto& fragment : line->m_fragments)
        {
            auto& col = tp.m_colors[(int)ThemeColor::TextGeneral + (int)fragment.m_fragType];
            fr.RenderText(renderer, fragment.m_chars, col, x + fragment.m_area.x, y, FontType::Text);
            maxWidth = Max(fragment.m_area.x + fragment.m_area.w, maxWidth);
        }
        y += LINE_HEIGHT;
    }
    m_clientContentSize.x = maxWidth + 32;

    SDL_SetRenderClipRect(renderer, &oldClipArea);
}

bool SearchWindow::HandleEvent(SDL_Event* e)
{
    switch (e->type)
    {
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        {
            int bx = (int)e->button.x;
            int by = (int)e->button.y;
            if (e->button.button == 1)
            {
                if (m_searchBox.Contains(bx, by))
                {
                    m_searchBox.Focus(true);
                    m_replaceBox.Focus(false);
                    m_searchBox.SetCursorPos(bx);
                    WindowManager::Instance().SetActiveWindow(this);
                }
                else if (m_replaceBox.Contains(bx, by))
                {
                    m_searchBox.Focus(false);
                    m_replaceBox.Focus(true);
                    m_searchBox.SetCursorPos(bx);
                    WindowManager::Instance().SetActiveWindow(this);
                }
                else if (m_searchLineArea.Contains(bx, by))
                {
                    int yBase = m_clientArea.h + LINE_HEIGHT * 3 + BORDER_MARGIN - m_clientContentOffset.y;
                    int line = (by - m_searchLineArea.y) / LINE_HEIGHT;
                    if (line >= 0 && line < m_searchLines.size())
                    {
                        auto sl = m_searchLines[line];
                        WindowTree* tree;
                        WindowLayout* layout;
                        WindowBase* window;
                        WindowManager::Instance().FindWindowByFile(m_searchFile, tree, layout, window);
                        if (window)
                        {
                            WindowMessageStruct wms;
                            wms.m_type = WindowMessage::Window_SetCursor;
                            wms.m_x = sl.m_startChar;
                            wms.m_y = sl.m_line;
                            window->MessageChild(layout, wms);
                            tree->m_dirty = true;
                        }
                    }
                }
            }
            return true;
        }
        break;

        case SDL_EVENT_MOUSE_MOTION:
        {
            if (e->button.button == 1)
            {
            }
        }
        break;

        case SDL_EVENT_MOUSE_BUTTON_UP:
        {
            if (e->button.button == 1)
            {
            }
        }
        break;

        case SDL_EVENT_KEY_DOWN:
            switch (e->key.key)
            {
                case SDLK_RETURN:
                    if (m_searchBox.IsFocused())
                    {
                        Search();
                    }
                    else if (m_replaceBox.IsFocused())
                    {
                        Search();
                        Replace();
                    }
                    return true;
                case SDLK_TAB:
                    if (m_searchBox.IsFocused())
                    {
                        m_searchBox.Focus(false);
                        m_replaceBox.Focus(true);
                    }
                    else
                    {
                        m_searchBox.Focus(true);
                        m_replaceBox.Focus(false);
                    }
                    break;
                default:
                    if (m_searchBox.IsFocused())
                    {
                        return m_searchBox.HandleEvent(e);
                    }
                    else if (m_replaceBox.IsFocused())
                    {
                        return m_replaceBox.HandleEvent(e);
                    }
                    break;
            }
            break;

        case SDL_EVENT_TEXT_INPUT:
            if (m_searchBox.IsFocused())
            {
                return m_searchBox.HandleEvent(e);
            }
            else if (m_replaceBox.IsFocused())
            {
                return m_replaceBox.HandleEvent(e);
            }
            break;
    }
    return WindowBase::HandleEvent(e);
}

bool SearchWindow::Tick()
{
    m_animTime += WINDOW_TICK_MS * (1.0f / 1000.0f);
    if (m_animTime > 1.0f)
        m_animTime -= 1.0f;
    m_searchBox.Tick();
    m_replaceBox.Tick();
    return true;
}

void SearchWindow::MessageChild(WindowLayout* layout, struct WindowMessageStruct& msg)
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

void SearchWindow::SaveTokens(std::vector<std::string>& layoutTokens)
{
    layoutTokens.push_back("SEARCH");
}

void SearchWindow::SetSearchText(const std::string& text)
{
    m_searchBox.m_text = text;
}

bool SearchWindow::CreateFromLayoutTokens(struct WindowLayout* layout, const std::vector<std::string>& layoutTokens, size_t& idx)
{
    if (layoutTokens[idx] != "SEARCH")
        return false;

    idx++;
    auto win = new SearchWindow();
    layout->m_tabs.push_back(win);
    return true;
}

void SearchWindow::Replace()
{
    if (m_searchFile && m_searchLines.size() > 0)
    {
        WindowTree* tree;
        WindowLayout* layout;
        WindowBase* window;
        WindowManager::Instance().FindWindowByFile(m_searchFile, tree, layout, window);
        if (window)
        {
            ((SourceFileWindow*)window)->ReplaceLines(m_searchLines, m_replaceBox.m_text);
        }
    }
}

void SearchWindow::Search()
{
    m_searchLines.clear();
    m_searchFile = SourceFileManager::Instance().GetActiveSourceFile();
    if (m_searchFile && !m_searchBox.m_text.empty())
    {
        for (int lineIdx = 0; lineIdx < m_searchFile->m_lines.size(); lineIdx++)
        {
            auto line = m_searchFile->m_lines[lineIdx];
            size_t foundIdx = line->m_chars.find(m_searchBox.m_text);
            if (foundIdx != std::string::npos)
            {
                SearchResult result;
                result.m_line = lineIdx;
                result.m_startChar = (int)foundIdx;
                result.m_length = (int)m_searchBox.m_text.size();
                m_searchLines.emplace_back(result);
            }
        }
    }
    m_clientContentSize.y = (int)m_searchLines.size() * LINE_HEIGHT;
    LayoutScrollbars();
}



