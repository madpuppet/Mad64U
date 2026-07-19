#include "common.h"
#include "UndoBufferWindow.h"
#include "SourceFileManager.h"
#include "FontRenderer.h"
#include "Application.h"
#include "SourceFileCmdBuffer.h"
#include <filesystem>
#include <unordered_map>
#include <vector>

void UndoBufferWindow::Paint(SDL_Renderer* renderer, const Recti& dirtyArea)
{
    auto& sm = SourceFileManager::Instance();
    auto file = sm.GetActiveSourceFile();
    if (!file)
        return;

    auto& fr = FontRenderer::Instance();
    auto& tp = Application::Instance().GetThemeProperties();

    // draw background
    auto window = WindowManager::Instance().GetActiveWindowBase();
    if (window == this)
        tp.SetRenderDrawColor(renderer, ThemeColor::SourceBackgroundSelected);
    else
        tp.SetRenderDrawColor(renderer, ThemeColor::SourceBackground);
    SDL_FRect body = m_clientArea.AsSDLFRect();
    SDL_RenderFillRect(renderer, &body);

    auto& list = file->m_cmdBuffer->m_commandList;
    m_clientContentSize.y = (int)list.size() * LINE_HEIGHT + LINE_HEIGHT;

    if (file->m_cmdBuffer->m_cmdIndex != m_lastVisibleRow)
    {
        MakeRowVisible(file->m_cmdBuffer->m_cmdIndex);
        m_lastVisibleRow = file->m_cmdBuffer->m_cmdIndex;
    }

    int firstLine = Max(m_clientContentOffset.y / LINE_HEIGHT, 0);
    int lastLine = Min(firstLine + m_clientArea.h / LINE_HEIGHT, (int)list.size());
    int xBase = m_clientArea.x - m_clientContentOffset.x + BORDER_MARGIN;
    int yBase = m_clientArea.y - m_clientContentOffset.y + BORDER_MARGIN;
    int x = xBase;
    int y = yBase + firstLine * LINE_HEIGHT;
    int w = 0;

    tp.SetRenderDrawColor(renderer, ThemeColor::Cursor);
    SDL_FRect selected{ (float)(xBase - BORDER_MARGIN), (float)(yBase + file->m_cmdBuffer->m_cmdIndex * LINE_HEIGHT), (float)m_clientArea.w, 4 };
    SDL_RenderFillRect(renderer, &selected);

    for (int i = firstLine; i < lastLine; i++)
    {
        auto line = list[i];
        Recti area;
        fr.CalcTextArea(renderer, line->Desc(), {0,0}, FontType::Text, area);
        w = Max(w, area.w);
        fr.RenderText(renderer, line->Desc(), tp.m_colors[(int)ThemeColor::TextGeneral], x, y, FontType::Text);
        y += LINE_HEIGHT;
    }
    m_clientContentSize.x = w;
}

UndoBufferWindow::UndoBufferWindow()
{
    m_name = "Undo";
}

UndoBufferWindow::~UndoBufferWindow()
{
}

bool UndoBufferWindow::HandleEvent(SDL_Event* e)
{
    return WindowBase::HandleEvent(e);
}

bool UndoBufferWindow::Tick()
{
    return false;
}

void UndoBufferWindow::SaveTokens(std::vector<std::string>& layoutTokens)
{
    layoutTokens.push_back("UNDO");
}

bool UndoBufferWindow::CreateFromLayoutTokens(WindowLayout* layout, const std::vector<std::string>& layoutTokens, size_t& idx)
{
    if (layoutTokens[idx] != "UNDO")
        return false;

    idx++;
    auto win = new UndoBufferWindow;
    layout->m_tabs.push_back(win);
    return true;
}

void UndoBufferWindow::MessageChild(WindowLayout* layout, struct WindowMessageStruct& msg)
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

