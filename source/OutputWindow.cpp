#include "common.h"
#include "OutputWindow.h"
#include "FontRenderer.h"
#include "LogManager.h"
#include "Application.h"

OutputWindow::OutputWindow()
{
    m_name = "Output";
}

void OutputWindow::Paint(SDL_Renderer* renderer, const Recti& dirtyArea)
{
    auto& fr = FontRenderer::Instance();
    auto& tp = Application::Instance().GetThemeProperties();
    auto& lm = LogManager::Instance();

    // update content size
    LogGroup logGroup = LogGroup::Build;
    lm.m_lock.lock();

    auto& lines = lm.m_logs[(int)logGroup].m_lines;
    m_clientContentSize.y = (int)lines.size() * LINE_HEIGHT + LINE_HEIGHT;
    LayoutScrollbars();

    // draw background
    auto window = WindowManager::Instance().GetActiveWindowBase();
    if (window == this)
        tp.SetRenderDrawColor(renderer, ThemeColor::SourceBackgroundSelected);
    else
        tp.SetRenderDrawColor(renderer, ThemeColor::SourceBackground);
    SDL_FRect body = m_clientArea.AsSDLFRect();
    SDL_RenderFillRect(renderer, &body);

    int firstLine = Max(m_clientContentOffset.y / LINE_HEIGHT, 0);
    int lastLine = Min(firstLine + m_clientArea.h / LINE_HEIGHT, (int)lines.size() - 1);
    int xBase = m_clientArea.x - m_clientContentOffset.x + BORDER_MARGIN;
    int yBase = m_clientArea.y - m_clientContentOffset.y + BORDER_MARGIN;
    int x = xBase;
    int y = yBase + firstLine * LINE_HEIGHT;
    int w = 0;
    for (int i = firstLine; i < lastLine; i++)
    {
        auto line = lines[i];
        Recti area;
        fr.CalcTextArea(renderer, line, {0,0}, FontType::Text, area);
        w = Max(w, area.w);
        fr.RenderText(renderer, line, tp.m_colors[(int)ThemeColor::TextGeneral], x, y, FontType::Text);
        y += LINE_HEIGHT;
    }

    m_clientContentSize.x = w;

    lm.m_lock.unlock();
}

bool OutputWindow::HandleEvent(SDL_Event* e)
{
    return WindowBase::HandleEvent(e);
}

void OutputWindow::SaveTokens(std::vector<std::string>& layoutTokens)
{
    layoutTokens.push_back("OUTPUT");
    layoutTokens.push_back(std::format("{}", (int)m_activeGroup));
}

bool OutputWindow::CreateFromLayoutTokens(WindowLayout* layout, const std::vector<std::string>& layoutTokens, size_t& idx)
{
    if (layoutTokens[idx] != "OUTPUT")
        return false;

    idx++;
    auto win = new OutputWindow;
    win->m_activeGroup = (LogGroup)std::stoi(layoutTokens[idx++]);
    layout->m_tabs.push_back(win);
    return true;
}



