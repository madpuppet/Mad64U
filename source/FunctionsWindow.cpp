#include "common.h"
#include "FunctionsWindow.h"
#include "SourceFileManager.h"
#include "FontRenderer.h"
#include "Application.h"
#include "SourceFileCmdBuffer.h"
#include <filesystem>
#include <unordered_map>
#include <vector>
#include "LogManager.h"

void FunctionsWindow::Paint(SDL_Renderer* renderer, const Recti& dirtyArea)
{
    auto& sm = SourceFileManager::Instance();
    auto file = sm.GetActiveSourceFile();
    if (!file)
        return;

    static int iii = 0;
    iii++;

    if (file->m_fileID != m_cachedFileID || file->m_lines.size() != m_cachedLinesSize)
    {
        RebuildLines();
        m_clientContentSize.y = (int)m_functionLines.size() * LINE_HEIGHT + LINE_HEIGHT;
        LayoutScrollbars();
    }

    auto& fr = FontRenderer::Instance();
    auto& tp = Application::Instance().GetThemeProperties();
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

    int firstLine = Max(m_clientContentOffset.y / LINE_HEIGHT, 0);
    int lastLine = Min(firstLine + m_clientArea.h / LINE_HEIGHT, (int)m_functionLines.size());
    int xBase = m_clientArea.x - m_clientContentOffset.x + BORDER_MARGIN;
    int yBase = m_clientArea.y - m_clientContentOffset.y + BORDER_MARGIN;
    int x = xBase;
    int y = yBase + firstLine * LINE_HEIGHT;
    int w = 0;
    for (int i = firstLine; i < lastLine; i++)
    {
        auto &fl = m_functionLines[i];
        fr.RenderText(renderer, std::format("{:4d}", fl.m_line), tp.m_colors[(int)ThemeColor::TextOperator], x, y, FontType::Text);

        Recti area;
        fr.CalcTextArea(renderer, fl.m_label, { 0,0 }, FontType::Text, area);
        w = Max(100 + w, area.w);

        ThemeColor textCol = ThemeColor::TextGeneral;
        if (fl.m_label[0] < 'A' || fl.m_label[0] > 'Z')
            textCol = ThemeColor::TextComment;

        fr.RenderText(renderer, fl.m_label, tp.m_colors[(int)textCol], x + 100, y, FontType::Text);

        if (highlight.m_highlight == WindowHighlightType::FunctionsWindow && highlight.m_window == this && highlight.m_functionsWindow.line == i)
        {
            SDL_FRect rectf{ (float)(x + 100), (float)y, (float)area.w, (float)area.h };
            tp.SetRenderDrawColor(renderer, ThemeColor::Cursor);
            SDL_RenderRect(renderer, &rectf);
        }
        y += LINE_HEIGHT;
    }
    m_clientContentSize.x = w;
}

FunctionsWindow::FunctionsWindow()
{
    m_name = "functions";
}

FunctionsWindow::~FunctionsWindow()
{
}

bool FunctionsWindow::HandleEvent(SDL_Event* e)
{
    switch (e->type)
    {
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        {
            auto& wm = WindowManager::Instance();
            auto& highlight = wm.GetWindowHighlightQuery();
            if (highlight.m_highlight == WindowHighlightType::FunctionsWindow && highlight.m_window == this)
            {
                auto file = SourceFileManager::Instance().FindFileByID(m_cachedFileID);
                if (file)
                {
                    int fileLine = m_functionLines[highlight.m_functionsWindow.line].m_line;
                    WindowTree* tree;
                    WindowLayout* layout;
                    WindowBase* window;
                    WindowManager::Instance().FindWindowByFile(file, tree, layout, window);
                    if (window)
                    {
                        WindowMessageStruct wms;
                        wms.m_type = WindowMessage::Window_SetCursor;
                        wms.m_x = 0;
                        wms.m_y = fileLine;
                        window->MessageChild(layout, wms);
                        tree->m_dirty = true;
                    }
                }
            }
        }
        break;
    }
    return WindowBase::HandleEvent(e);
}

bool FunctionsWindow::Tick()
{
    return false;
}

void FunctionsWindow::SaveTokens(std::vector<std::string>& layoutTokens)
{
    layoutTokens.push_back("FUNCS");
}

bool FunctionsWindow::CreateFromLayoutTokens(WindowLayout* layout, const std::vector<std::string>& layoutTokens, size_t& idx)
{
    if (layoutTokens[idx] != "FUNCS")
        return false;

    idx++;
    auto win = new FunctionsWindow;
    layout->m_tabs.push_back(win);
    return true;
}

void FunctionsWindow::MessageChild(WindowLayout* layout, struct WindowMessageStruct& msg)
{
    switch (msg.m_type)
    {
        case WindowMessage::Query_Highlight:
        {
            auto query = (WindowHighlightQuery*)msg.m_query;
            if (m_clientArea.Contains(msg.m_x, msg.m_y))
            {
                msg.m_response++;
                query->m_tree = msg.m_tree;
                query->m_layout = layout;
                query->m_window = this;

                int l = (msg.m_y - (m_clientArea.y - m_clientContentOffset.y)) / LINE_HEIGHT;
                if (l >= 0 && l < m_functionLines.size())
                {
                    auto& fr = FontRenderer::Instance();
                    Recti area;
                    fr.CalcTextArea(msg.m_tree->m_renderer, m_functionLines[l].m_label, {m_clientArea.x - m_clientContentOffset.x, m_clientArea.y - m_clientContentOffset.y}, FontType::Text, area);
                    query->m_area = area;
                    query->m_highlight = WindowHighlightType::FunctionsWindow;
                    query->m_functionsWindow.line = l;
                }
                else
                {
                    query->m_area = m_clientArea;
                    query->m_highlight = WindowHighlightType::ClientArea;
                }
                return;
            }
        }
        break;
    }
}

extern bool IsAlphaNumeric(char ch);
void FunctionsWindow::RebuildLines()
{
    auto& sm = SourceFileManager::Instance();
    auto file = sm.GetActiveSourceFile();
    if (!file)
        return;

    m_cachedFileID = file->m_fileID;
    m_cachedLinesSize = (int)file->m_lines.size();
    m_functionLines.clear();

    for (int i = 0; i < file->m_lines.size(); i++)
    {
        auto line = file->m_lines[i];
        if (line->m_chars.empty())
            continue;

        int c = 0;
        while (IsAlphaNumeric(line->m_chars[c]))
            c++;

        if (c > 0 && line->m_chars[c] == ':')
        {
            FunctionLine fl;
            fl.m_label = line->m_chars.substr(0, c);
            fl.m_line = i;
            m_functionLines.push_back(fl);
        }
    }
}
