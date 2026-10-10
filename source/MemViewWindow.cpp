#include "common.h"
#include "MemViewWindow.h"
#include "SourceFileManager.h"
#include "FontRenderer.h"
#include "Application.h"
#include "SourceFileWindow.h"
#include <filesystem>
#include <vector>
#include "ViceBridge.h"

#define MEMVIEW_BYTES_PER_ROW 32

void MemViewWindow::CalcModeInfo()
{
    static int BytesPerLine[] = {
        MEMVIEW_BYTES_PER_ROW
    };

    m_bytesPerLine = BytesPerLine[(int)m_memViewMode];
    m_lineCount = 65536 / m_bytesPerLine;
}

void MemViewWindow::Paint(SDL_Renderer* renderer, const Recti& dirtyArea)
{
    auto& sm = SourceFileManager::Instance();
    auto& fr = FontRenderer::Instance();
    auto& tp = Application::Instance().GetThemeProperties();
    auto& ir = IconRenderer::Instance();
    auto& wm = WindowManager::Instance();
    auto& highlight = wm.GetWindowHighlightQuery();
    ViceState vs = ViceBridge::Instance().GetViceState();
    u8* ram = ViceBridge::Instance().GetRam();

    // draw background
    auto window = WindowManager::Instance().GetActiveWindowBase();
    if (window == this)
        tp.SetRenderDrawColor(renderer, ThemeColor::SourceBackgroundSelected);
    else
        tp.SetRenderDrawColor(renderer, ThemeColor::SourceBackground);
    SDL_FRect body = m_clientArea.AsSDLFRect();
    SDL_RenderFillRect(renderer, &body);

    int firstLine = Max(m_clientContentOffset.y / LINE_HEIGHT, 0);
    int lastLine = Min(firstLine + m_clientArea.h / LINE_HEIGHT, m_lineCount);
    int xBase = m_clientArea.x - m_clientContentOffset.x + BORDER_MARGIN;
    int yBase = m_clientArea.y - m_clientContentOffset.y + BORDER_MARGIN;
    int x = xBase;
    int y = yBase + firstLine * LINE_HEIGHT;
    int w = 0;
    for (int i = firstLine; i < lastLine; i++)
    {
        int memAddr = i * MEMVIEW_BYTES_PER_ROW;
        fr.RenderText(renderer, std::format("{:04x}", i * m_bytesPerLine), tp.m_colors[(int)ThemeColor::TextOperator], x, y, FontType::Text);
        for (int j = 0; j < m_bytesPerLine; j++)
        {
            SDL_Color col = tp.m_colors[(int)ThemeColor::TextGeneral];
            int memAddr = i * m_bytesPerLine + j;
            if ((memAddr & 7) != 0)
                col.g = col.g * 3 / 4;
            if (i & 1)
                col.b = col.b * 3 / 4;
            fr.RenderText(renderer, std::format("{:02x}", ram[memAddr]), col, x + 100 + j * 30, y, FontType::Text);
        }
        y += LINE_HEIGHT;
    }

    m_clientContentSize.x = 100 + m_bytesPerLine * 30;
    m_clientContentSize.y = m_lineCount * LINE_HEIGHT;

    LayoutScrollbars();
}

MemViewWindow::MemViewWindow()
{
    m_name = "MemView";
    CalcModeInfo();
}

MemViewWindow::~MemViewWindow()
{}

bool MemViewWindow::HandleEvent(SDL_Event* e)
{
    return WindowBase::HandleEvent(e);
}

bool MemViewWindow::Tick()
{
    return false;
}

void MemViewWindow::SaveTokens(std::vector<std::string>& layoutTokens)
{
    layoutTokens.push_back("MEMVIEW");
}

bool MemViewWindow::CreateFromLayoutTokens(WindowLayout* layout, const std::vector<std::string>& layoutTokens, size_t& idx)
{
    if (layoutTokens[idx] != "MEMVIEW")
        return false;

    idx++;
    auto win = new MemViewWindow;
    layout->m_tabs.push_back(win);
    return true;
}

void MemViewWindow::MessageChild(WindowLayout* layout, struct WindowMessageStruct& msg)
{
    switch (msg.m_type)
    {
        case WindowMessage::File_Added:
        case WindowMessage::File_Deleted:
        case WindowMessage::File_Compiled:
        case WindowMessage::File_Renamed:
            break;

        case WindowMessage::Query_Highlight:
        {
            auto& fr = FontRenderer::Instance();
            auto& ir = IconRenderer::Instance();
            auto query = (WindowHighlightQuery*)msg.m_query;
            int firstLine = Max(m_clientContentOffset.y / LINE_HEIGHT, 0);
            int lastLine = Min(firstLine + m_clientArea.h / LINE_HEIGHT, m_lineCount);
            int xBase = m_clientArea.x - m_clientContentOffset.x + BORDER_MARGIN;
            int yBase = m_clientArea.y - m_clientContentOffset.y + BORDER_MARGIN;
            int mouseLine = (msg.m_y - yBase) / LINE_HEIGHT;
            int x = xBase;
            int y = yBase + mouseLine * LINE_HEIGHT;
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
