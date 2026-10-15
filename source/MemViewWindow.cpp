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
#define MEMVIEW_X_OFFSET 100
#define MEMVIEW_X_BYTE_WIDTH 30

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
    u64* heatMap = ViceBridge::Instance().GetHeatMap();

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
            if (m_breakpointID != 0 && memAddr == m_breakpointAddr)
            {
                SDL_FRect body;
                body.x = (float)(xBase + MEMVIEW_X_OFFSET + j * MEMVIEW_X_BYTE_WIDTH);
                body.y = (float)(yBase + i * LINE_HEIGHT);
                body.w = MEMVIEW_X_BYTE_WIDTH * 2 / 3;
                body.h = LINE_HEIGHT - 2;
                SDL_SetRenderDrawColor(renderer, 255, 0, 0, 128);
                SDL_RenderFillRect(renderer, &body);
            }

            if ((memAddr & 7) != 0)
                col.g = col.g * 3 / 4;
            if (i & 1)
                col.b = col.b * 3 / 4;

            int age = (int)(vs.m_clock - heatMap[memAddr]);
            col.r = 255 - Clamp(age / 10000, 0, 255);
            fr.RenderText(renderer, std::format("{:02x}", ram[memAddr]), col, x + MEMVIEW_X_OFFSET + j * MEMVIEW_X_BYTE_WIDTH, y, FontType::Text);
        }
        y += LINE_HEIGHT;
    }

    if (highlight.m_window == this && highlight.m_highlight == WindowHighlightType::MemoryAddress)
    {
        int row = highlight.m_id / MEMVIEW_BYTES_PER_ROW;
        if (row >= firstLine && row < lastLine)
        {
            int col = highlight.m_id % MEMVIEW_BYTES_PER_ROW;
            SDL_FRect body;
            body.x = (float)(xBase + MEMVIEW_X_OFFSET + col * MEMVIEW_X_BYTE_WIDTH);
            body.y = (float)(yBase + row * LINE_HEIGHT);
            body.w = MEMVIEW_X_BYTE_WIDTH * 2 / 3;
            body.h = LINE_HEIGHT - 2;
            SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
            SDL_RenderRect(renderer, &body);

            SDL_FRect addressBody;
            addressBody.x = body.x - body.w / 2.0f;
            addressBody.w = body.w * 2.0f;
            addressBody.y = body.y + LINE_HEIGHT * 1.1f;
            addressBody.h = LINE_HEIGHT;
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 128);
            SDL_RenderFillRect(renderer, &addressBody);
            SDL_Color addrCol = SDL_Color(255, 255, 0, 255);
            fr.RenderText(renderer, std::format("{:04x}", highlight.m_id), addrCol, (int)addressBody.x, (int)addressBody.y, FontType::Text);
        }
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

void MemViewWindow::CursorToRowCol(int x, int y, int& row, int& col)
{
    row = (y - (m_clientArea.y + BORDER_MARGIN) + m_clientContentOffset.y) / LINE_HEIGHT;
    col = (x - (m_clientArea.x + BORDER_MARGIN + MEMVIEW_X_OFFSET) + m_clientContentOffset.x) / MEMVIEW_X_BYTE_WIDTH;
}

bool MemViewWindow::HandleEvent(SDL_Event* e)
{
    switch (e->type)
    {
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        {
            if (e->button.button == 1)
            {
                int row, col;
                CursorToRowCol((int)e->button.x, (int)e->button.y, row, col);
                int maxRows = 65536 / MEMVIEW_BYTES_PER_ROW;
                if (row >= 0 && row < maxRows && col >= 0 && col < MEMVIEW_BYTES_PER_ROW)
                {
                    u16 memAddress = row * MEMVIEW_BYTES_PER_ROW + col;
                    bool setBP = true;
                    if (m_breakpointID != 0)
                    {
                        ViceBridge::Instance().ClearBreakpoint(m_breakpointID);
                        setBP = (m_breakpointAddr != memAddress);
                        m_breakpointID = 0;
                    }
                    if (setBP)
                    {
                        m_breakpointID = ViceBridge::Instance().SetBreakpoint(0, 0, memAddress, false);
                        m_breakpointAddr = memAddress;
                    }
                }
            }
            return true;
        }
        break;
    }

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
            if (m_clientArea.Contains(msg.m_x, msg.m_y))
            {
                int row, col;
                CursorToRowCol(msg.m_x, msg.m_y, row, col);
                auto& fr = FontRenderer::Instance();
                auto query = (WindowHighlightQuery*)msg.m_query;
                int maxRows = 65536 / MEMVIEW_BYTES_PER_ROW;
                msg.m_response++;
                query->m_area = m_clientArea;
                query->m_highlight = WindowHighlightType::ClientArea;
                query->m_tree = msg.m_tree;
                query->m_layout = layout;
                query->m_window = this;

                if (row >= 0 && row < maxRows && col >= 0 && col < MEMVIEW_BYTES_PER_ROW)
                {
                    query->m_id = row * MEMVIEW_BYTES_PER_ROW + col;
                    query->m_highlight = WindowHighlightType::MemoryAddress;
                }
                return;
            }
        }
        break;
    }
}
