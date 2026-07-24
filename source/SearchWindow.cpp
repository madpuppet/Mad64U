#include "common.h"
#include "SearchWindow.h"
#include "SourceFileManager.h"
#include "Application.h"
#include "WindowManager.h"
#include "FontRenderer.h"
#include "IconRenderer.h"

void TextBox::Enable(const std::string title, const Recti& area)
{
    m_stringPos = { m_area.x, m_area.y };
    m_title = title;
    m_area = { area.x + 200, area.y, area.w - 200, area.h };
    m_visible = true;
}
void TextBox::Disable()
{
    m_visible = false;
}
void TextBox::Paint(SDL_Renderer* renderer, float m_animateTimer)
{
    if (!m_visible)
        return;

    auto& fr = FontRenderer::Instance();
    auto& tp = Application::Instance().GetThemeProperties();

    SDL_Color backColor, titleColor, textColor;
    backColor = tp.Col(ThemeColor::SearchTextBack);
    titleColor = tp.Col(ThemeColor::SearchTitle);

    SDL_FRect backArea = m_area.AsSDLFRect();
    SDL_SetRenderDrawColor(renderer, backColor.r, backColor.g, backColor.b, backColor.a);
    SDL_RenderFillRect(renderer, &backArea);
    fr.RenderText(renderer, m_title, textColor, m_stringPos.x, m_stringPos.y, FontType::UI);

    // draw cusror
    if (m_focused)
    {
    }
}

void TextBox::HandleEvent(SDL_Event* e)
{
}


SearchWindow::SearchWindow()
{
    m_name = "Search";
}

SearchWindow::~SearchWindow()
{
}

void SearchWindow::SetMode(SearchMode mode)
{
    m_searchMode = mode;
}

void SearchWindow::Paint(SDL_Renderer* renderer, const Recti& dirtyArea)
{
    m_clientContentSize.y = 4 * LINE_HEIGHT;

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

    SDL_FRect sarArea;
    int sarLines = 0;
    std::string header;
    switch (m_searchMode)
    {
        case SearchMode::Goto:
            header = "Goto Line";
            m_searchBox.Enable("Goto", { m_clientArea.x + BORDER_MARGIN, m_clientArea.y + BORDER_MARGIN + LINE_HEIGHT, m_clientArea.w - BORDER_MARGIN * 2, LINE_HEIGHT });
            m_replaceBox.Disable();
            sarArea = { (float)m_clientArea.x, (float)m_clientArea.y, (float)m_clientArea.w, (float)LINE_HEIGHT };
            sarLines = 2;
            break;
        case SearchMode::Replace:
            header = "Search and Replace";
            m_searchBox.Enable("Search", { m_clientArea.x + BORDER_MARGIN, m_clientArea.y + LINE_HEIGHT + BORDER_MARGIN, m_clientArea.w - BORDER_MARGIN * 2, LINE_HEIGHT });
            m_searchBox.Enable("Replace", { m_clientArea.x + BORDER_MARGIN, m_clientArea.y + LINE_HEIGHT + BORDER_MARGIN * 2 + LINE_HEIGHT, m_clientArea.w - BORDER_MARGIN * 2, LINE_HEIGHT });
            sarArea = { (float)m_clientArea.x, (float)m_clientArea.y, (float)m_clientArea.w, (float)LINE_HEIGHT * 2 };
            sarLines = 3;
            break;
        case SearchMode::Search:
            header = "Search";
            m_searchBox.Enable("Search", { m_clientArea.x + BORDER_MARGIN, m_clientArea.y + LINE_HEIGHT + BORDER_MARGIN, m_clientArea.w - BORDER_MARGIN * 2, LINE_HEIGHT });
            m_replaceBox.Disable();
            sarArea = { (float)m_clientArea.x, (float)m_clientArea.y, (float)m_clientArea.w, (float)LINE_HEIGHT };
            sarLines = 2;
            break;
    }

    tp.SetRenderDrawColor(renderer, ThemeColor::SearchTitleBack);
    SDL_RenderFillRect(renderer, &sarArea);

    m_searchBox.Paint(renderer, m_animTime);
    m_replaceBox.Paint(renderer, m_animTime);

    int firstLine = Max(m_clientContentOffset.y / LINE_HEIGHT, 0);
    int lastLine = Min(firstLine + m_clientArea.h / LINE_HEIGHT + 2, (int)m_results.size());
    int xBase = m_clientArea.x - m_clientContentOffset.x + BORDER_MARGIN;
    int yBase = m_clientArea.y - m_clientContentOffset.y + BORDER_MARGIN;
    int x = xBase;
    int y = yBase + firstLine * LINE_HEIGHT;
    int w = 0;
    for (int i = firstLine; i < lastLine; i++)
    {
        auto result = m_results[i];
        std::string line = result.m_sourceFile->m_lines[result.m_line]->m_chars;
        Recti area;
        fr.CalcTextArea(renderer, line, { 0,0 }, FontType::Text, area);
        w = Max(w, area.w);
        fr.RenderText(renderer, line, tp.m_colors[(int)ThemeColor::TextGeneral], x, y, FontType::Text);
        y += LINE_HEIGHT;
    }
}

bool SearchWindow::HandleEvent(SDL_Event* e)
{
    return true;
}

bool SearchWindow::Tick()
{
    m_animTime += WINDOW_TICK_MS * (1.0f / 1000.0f);
    if (m_animTime > 1.0f)
        m_animTime -= 1.0f;
    return true;
}

void SearchWindow::MessageChild(WindowLayout* layout, struct WindowMessageStruct& msg)
{
}

void SearchWindow::SaveTokens(std::vector<std::string>& layoutTokens)
{
    layoutTokens.push_back("SEARCH");
}

bool SearchWindow::CreateFromLayoutTokens(struct WindowLayout* layout, const std::vector<std::string>& layoutTokens, size_t& idx)
{
    if (layoutTokens[idx] != "SEARCH")
        return false;

    auto win = new SearchWindow();
    layout->m_tabs.push_back(win);
}


