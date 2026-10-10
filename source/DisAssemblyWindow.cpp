#include "common.h"
#include "DisAssemblyWindow.h"
#include "SourceFileManager.h"
#include "FontRenderer.h"
#include "Application.h"
#include "SourceFileWindow.h"
#include <filesystem>
#include <vector>
#include "ViceBridge.h"
#include "cpu6502.h"

#define MAX_DISASSEMBLY_LINES 32

void DisAssemblyWindow::Paint(SDL_Renderer* renderer, const Recti& dirtyArea)
{
    auto& sm = SourceFileManager::Instance();
    auto& fr = FontRenderer::Instance();
    auto& tp = Application::Instance().GetThemeProperties();
    auto& ir = IconRenderer::Instance();
    auto& wm = WindowManager::Instance();
    auto& highlight = wm.GetWindowHighlightQuery();
    ViceState vs = ViceBridge::Instance().GetViceState();
    u8* ram = ViceBridge::Instance().GetRam();

    if (vs.m_pc != m_cacheAddr)
    {
        m_cacheAddr = vs.m_pc;
        BuildLines();
    }

    // draw background
    auto window = WindowManager::Instance().GetActiveWindowBase();
    if (window == this)
        tp.SetRenderDrawColor(renderer, ThemeColor::SourceBackgroundSelected);
    else
        tp.SetRenderDrawColor(renderer, ThemeColor::SourceBackground);
    SDL_FRect body = m_clientArea.AsSDLFRect();
    SDL_RenderFillRect(renderer, &body);

    int firstLine = Max(m_clientContentOffset.y / LINE_HEIGHT, 0);
    int lastLine = Min(firstLine + m_clientArea.h / LINE_HEIGHT, (int)m_lines.size());
    int xBase = m_clientArea.x - m_clientContentOffset.x + BORDER_MARGIN;
    int yBase = m_clientArea.y - m_clientContentOffset.y + BORDER_MARGIN;
    int x = xBase;
    int y = yBase + firstLine * LINE_HEIGHT;
    int w = 0;
    for (int i = firstLine; i < lastLine; i++)
    {
        auto& ln = m_lines[i];
       
        if (ln.addr == vs.m_pc)
        {
            SDL_FRect body = { (float)m_clientArea.x, (float)y, (float)m_clientArea.w, (float)LINE_HEIGHT };
            tp.SetRenderDrawColor(renderer, ThemeColor::TextHighlight);
            SDL_RenderFillRect(renderer, &body);
        }

        x = xBase;
        fr.RenderText(renderer, std::format("{:04x}", ln.addr), tp.m_colors[(int)ThemeColor::TextLabel], x, y, FontType::Text);
        x += 50;

        for (int ii = 0; ii < ln.bytes; ii++)
        {
            fr.RenderText(renderer, std::format("{:02x}", ram[ln.addr+ii]), tp.m_colors[(int)ThemeColor::TextLabel], x + ii * 30, y, FontType::Text);
        }
        x += 100;
        fr.RenderText(renderer, std::format("{:02x}", ln.cycles), tp.m_colors[(int)ThemeColor::TextLabel], x, y, FontType::Text);
        x += 30;
        fr.RenderText(renderer, m_lines[i].opcodeStr, tp.m_colors[(int)ThemeColor::TextOperator], x, y, FontType::Text);
        fr.RenderText(renderer, m_lines[i].operandStr, tp.m_colors[(int)ThemeColor::TextGeneral], x + 50, y, FontType::Text);
        y += LINE_HEIGHT;
    }

    m_clientContentSize.x = 200;
    m_clientContentSize.y = (int)m_lines.size() * LINE_HEIGHT;

    LayoutScrollbars();
}

DisAssemblyWindow::DisAssemblyWindow()
{
    m_name = "DisAssembly";
}

DisAssemblyWindow::~DisAssemblyWindow()
{}

bool DisAssemblyWindow::HandleEvent(SDL_Event* e)
{
    return WindowBase::HandleEvent(e);
}

bool DisAssemblyWindow::Tick()
{
    return false;
}

void DisAssemblyWindow::SaveTokens(std::vector<std::string>& layoutTokens)
{
    layoutTokens.push_back("DISASSEMBLY");
}

bool DisAssemblyWindow::CreateFromLayoutTokens(WindowLayout* layout, const std::vector<std::string>& layoutTokens, size_t& idx)
{
    if (layoutTokens[idx] != "DISASSEMBLY")
        return false;

    idx++;
    auto win = new DisAssemblyWindow;
    layout->m_tabs.push_back(win);
    return true;
}

void DisAssemblyWindow::MessageChild(WindowLayout* layout, struct WindowMessageStruct& msg)
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
            int lastLine = Min(firstLine + m_clientArea.h / LINE_HEIGHT, (int)m_lines.size());
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

inline bool IsAssembly(Cpu6502 &cpu, u16 addr, u8* ram, u64* touchRam, int wantSize)
{
    auto op = cpu.GetOpcode(ram[addr]);
    return (op->size == wantSize && op->cycles != 1 && touchRam[addr] != 0);
}

inline bool IsAssembly(Cpu6502 &cpu, u16 addr, u8* ram, int wantSize)
{
    auto op = cpu.GetOpcode(ram[addr]);
    return (op->size == wantSize && op->cycles != 1);
}

int DisAssemblyWindow::DisassembleLine(Cpu6502 &cpu, u8 *ram, int l, u16 addr)
{
    Line& line = m_lines[l];
    cpu.Disassemble(addr, ram[addr], ram[(u16)(addr+1)], ram[(u16)(addr+2)], line.opcodeStr, line.operandStr, line.bytes, line.cycles);
    line.addr = addr;
    return line.bytes;
}

static bool stopme = false;

void DisAssemblyWindow::BuildLines()
{
    auto& vb = ViceBridge::Instance();
    ViceState state = vb.GetViceState();
    u8* ram = vb.GetRam();
    u64* pcTouchRam = vb.GetPCTouchRam();
    auto& cpu = Cpu6502::Instance();

    // trace up to the top
    m_lines.resize(MAX_DISASSEMBLY_LINES);

    int addr = state.m_pc;
    int l = MAX_DISASSEMBLY_LINES / 2;

    if (addr == 0x811)
        stopme = true;

    DisassembleLine(cpu, ram, l, addr);

    for (int i = l-1; i >= 0; i--)
    {
        bool found = false;
        for (int ii = 1; ii < 4; ii++)
        {
            if (IsAssembly(cpu, (u16)(addr - ii), ram, pcTouchRam, ii))
            {
                DisassembleLine(cpu, ram, i, (u16)(addr - ii));
                addr -= ii;
                found = true;
                break;
            }
        }
        if (!found)
        {
            for (int ii = 1; ii < 4; ii++)
            {
                if (IsAssembly(cpu, (u16)(addr - ii), ram, ii))
                {
                    DisassembleLine(cpu, ram, i, (u16)(addr - ii));
                    addr -= ii;
                    found = true;
                    break;
                }
            }
        }
        if (!found)
        {
            DisassembleLine(cpu, ram, i, (u16)(addr - 3));
            addr -= 3;
        }
    }

    addr = state.m_pc + m_lines[l].bytes;
    for (int i = l+1; i < MAX_DISASSEMBLY_LINES; i++)
    {
        addr += DisassembleLine(cpu, ram, i, addr);
    }
}

