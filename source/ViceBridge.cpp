#include "common.h"
#include "ViceBridge.h"
#include "LogManager.h"
#include "SourceFileManager.h"
#include "WindowManager.h"

extern "C" int helper_set_video_mode(int pal);
extern "C" uint64_t helper_get_clock_cycle();
extern "C" void helper_get_raster_pos(unsigned int* rasterLine, unsigned int* rasterCycle);
extern "C" int helper_suspend_sound();
extern "C" int helper_resume_sound();

//==================================================================================================================================
// Emulation internals
//==================================================================================================================================

struct CPUBreakpoint
{
    int m_id = 0;
    int m_fileID = 0;
    int m_lineID = 0;
    u16 m_addr = 0;
    bool m_once = false;
};
struct TracePoint
{
    u64 clock;
    u16 raster;
    u16 addr;
    u8 cycle;
    u8 a;
    u8 x;
    u8 y;
    u8 flags;
    u8 sp;
};

#define MAX_TRACE_POINTS (1024*1024)
struct EmulationState
{
    uint64_t m_clock = 0;
    int m_runCycles = -1;
    std::vector<CPUBreakpoint> m_breakpoints;
    TracePoint m_trace[MAX_TRACE_POINTS];
    int m_traceCurrIdx = 0;
    int m_traceFirstIdx = 0;
    u8 m_ram[65536];
    u64 m_pcTouch[65536];
    u64 m_heatMap[65536];

    EmulationState()
    {
        memset(m_ram, 0, sizeof(m_ram));
        memset(m_pcTouch, 0, sizeof(m_pcTouch));
        memset(m_heatMap, 0, sizeof(m_heatMap));
        memset(m_trace, 0, sizeof(m_trace));
    }

    void ClearOnceBreakpoints(u16 addr)
    {
        std::erase_if(m_breakpoints, [addr](const CPUBreakpoint& bp)
            {
                return bp.m_once && bp.m_addr == addr;
            });
    }
};
EmulationState s_emulationState;

void CheckBreakpoints(u16 addr)
{
    // check any breakpoints...
    for (auto bp : s_emulationState.m_breakpoints)
    {
        if (bp.m_addr == addr)
        {
            auto& vb = ViceBridge::Instance();

            vb.SetViceStopped();

            // alert that we have hit a permanent breakpoint
            vb.BreakpointHit(bp);

            // clear all oneshot breakpoints at this addr
            s_emulationState.ClearOnceBreakpoints(addr);

            // busy wait till we get a cmd from the editor to run some more emulation
            vb.ProcessViceCmdsTillContinue();
            break;
        }
    }
}

extern "C" void mad64_update_ram(u8 *ram)
{
    memcpy(s_emulationState.m_ram, ram, 65536);
}

extern "C" void mad64_track_ram_store(u64 clock, u16 addr)
{
    s_emulationState.m_heatMap[addr] = clock;
    CheckBreakpoints(addr);
}

extern "C" void mad64_process_cycle(u64 clock, u16 pc_addr, u8 a, u8 x, u8 y, u8 flags, u8 sp)
{
    // add this trace point
    int nextIdx = (s_emulationState.m_traceCurrIdx + 1) & (MAX_TRACE_POINTS - 1);
    if (nextIdx == s_emulationState.m_traceFirstIdx)
    {
        s_emulationState.m_traceFirstIdx = (s_emulationState.m_traceFirstIdx + 1) & (MAX_TRACE_POINTS - 1);
    }

    u32 rasterLine, rasterCycle;
    helper_get_raster_pos(&rasterLine, &rasterCycle);
    auto& tp = s_emulationState.m_trace[nextIdx];
    tp.clock = clock;
    tp.raster = (u16)rasterLine;
    tp.cycle = (u8)rasterCycle;
    tp.addr = pc_addr;
    tp.a = a;
    tp.x = x;
    tp.y = y;
    tp.flags = flags;
    tp.sp = sp;
    s_emulationState.m_pcTouch[pc_addr] = clock;
    s_emulationState.m_traceCurrIdx = nextIdx;

    if (s_emulationState.m_runCycles != -1 && (--s_emulationState.m_runCycles == 0))
    {
        auto& vb = ViceBridge::Instance();

        vb.SetViceStopped();
        s_emulationState.m_runCycles = -1;

        auto cmd = new VBC_BreakPointHit;
        ViceBridge::Instance().SendVice2Mad(cmd);

        // clear all oneshot breakpoints at this addr
        s_emulationState.ClearOnceBreakpoints(pc_addr);

        // busy wait till we get a cmd from the editor to run some more emulation
        vb.ProcessViceCmdsTillContinue();
    }
    else
        CheckBreakpoints(pc_addr);
}

int ViceBridge::SetBreakpoint(int fileID, int lineID, u16 addr, bool oneShot)
{
    int breakpointID = m_nextBreakpointID++;
    auto cmd = new VBC_SetBreakpoint;
    cmd->m_fileID = fileID;
    cmd->m_lineID = lineID;
    cmd->m_addr = addr;
    cmd->m_breakpointID = breakpointID;
    cmd->m_oneShot = oneShot;
    SendMad2Vice(cmd);
    return breakpointID;
}

ViceBridge::ViceBridge()
{
    auto msgLoop = [this]()
        {
            for (;;)
            {
                m_signalMsg.acquire();
                m_cmdMutex.lock();
                auto cmd = m_cmds.front();
                m_cmds.pop_front();
                m_cmdMutex.unlock();
                cmd->Execute();
                delete cmd;
            }
        };
    std::thread(msgLoop).detach();
}

extern "C" void mad64_video_refresh(u8* buffer, int width, int height, int firstLine, int lastLine)
{
    if (width == 0 || height == 0)
        return;

    auto cmd = new VBC_NewFrame;
    cmd->m_buffer = new u8[width * height];
    cmd->m_height = height;
    cmd->m_width = width;
    cmd->m_firstLine = firstLine;
    cmd->m_lastLine = lastLine;
    memcpy(cmd->m_buffer, buffer, width * height);
    ViceBridge::Instance().Queue(cmd);
}

extern "C" void mad64_process_vice_commands(void)
{
    ViceBridge::Instance().ExecuteViceCmds();
}

static uint64_t s_clock_start = 0;
static uint64_t s_clock_elapsed = 0;

void ViceBridge::ProcessViceCmdsTillContinue()
{
    while (m_vice_stopped)
    {
        Sleep(1);
        ExecuteViceCmds();
    }
}

ViceState ViceBridge::GetViceState()
{
    int idx = s_emulationState.m_traceCurrIdx;
    TracePoint& tp = s_emulationState.m_trace[idx];
    ViceState state;
    state.m_acc = tp.a;
    state.m_pc = tp.addr;
    state.m_flags = tp.flags;
    state.m_x = tp.x;
    state.m_y = tp.y;
    state.m_rasterCycle = 0;
    state.m_rasterLine = 0;
    state.m_clock = tp.clock;
    return state;
}

u8* ViceBridge::GetRam()
{
    return s_emulationState.m_ram;
}

u64* ViceBridge::GetPCTouchRam()
{
    return s_emulationState.m_pcTouch;
}

void ViceBridge::BreakpointHit(const CPUBreakpoint &bp)
{
    auto cmd = new VBC_BreakPointHit;
    cmd->m_breakpointID = bp.m_id;
    cmd->m_fileID = bp.m_fileID;
    cmd->m_lineID = bp.m_lineID;
    SendVice2Mad(cmd);
}

void VBC_BreakPointHit::Execute()
{
    int fileID = m_fileID ? m_fileID : ViceBridge::Instance().GetActiveFileID();

    WindowMessageStruct msgBH;
    msgBH.m_type = WindowMessage::Window_BreakpointHit;
    msgBH.m_flags = WMF_EarlyOut | WMF_Window | WMF_TabActive;
    msgBH.m_sourceFile = SourceFileManager::Instance().FindFileByID(fileID);
    if (msgBH.m_sourceFile)
    {
        WindowManager::Instance().Message(msgBH);
    }
}

void ViceBridge::MultiStep()
{
    auto cmd = new VBC_Continue;
    cmd->m_steps = 20;
    SendMad2Vice(cmd);
}

void ViceBridge::SingleStep()
{
    auto cmd = new VBC_Continue;
    cmd->m_steps = 1;
    SendMad2Vice(cmd);
}

void ViceBridge::Continue()
{
    auto cmd = new VBC_Continue;
    cmd->m_steps = 0;
    SendMad2Vice(cmd);
}

void ViceBridge::Pause()
{
    SendMad2Vice(new VBC_Pause);
}

void VBC_Continue::Execute()
{
    ViceBridge::Instance().ClearViceStopped();
    s_emulationState.m_runCycles = m_steps;
}

void VBC_Pause::Execute()
{
    ViceBridge::Instance().SetViceStopped();
    ViceBridge::Instance().ProcessViceCmdsTillContinue();
}

void VBC_SetVideoStandard::Execute()
{
    helper_set_video_mode(m_palMode);
}

void ViceBridge::SetViceStopped()
{
    m_vice_stopped = true;
    helper_suspend_sound();
}

void ViceBridge::ClearViceStopped()
{
    m_vice_stopped = false;
    helper_resume_sound();
}


void ViceBridge::ExecuteViceCmds()
{
    std::vector<ViceBridgeCmd*> runme;
    m_mutexMad2Vice.lock();
    for (auto cmd : m_syncMad2Vice)
    {
        runme.push_back(cmd);
    }
    m_syncMad2Vice.clear();
    m_mutexMad2Vice.unlock();

    for (auto cmd : runme)
    {
        cmd->Execute();
        delete cmd;
    }
}

void ViceBridge::ExecuteMadCmds()
{
    std::vector<ViceBridgeCmd*> runme;
    m_mutexVice2Mad.lock();
    for (auto cmd : m_syncVice2Mad)
    {
        runme.push_back(cmd);
    }
    m_syncVice2Mad.clear();
    m_mutexVice2Mad.unlock();

    for (auto cmd : runme)
    {
        cmd->Execute();
        delete cmd;
    }
}


static u32 c64Palette[16] = {
    0xFF000000, //  0: Black
    0xFFFFFFFF, //  1: White
    0xFF68372B, //  2: Red
    0xFF70A4B2, //  3: Cyan
    0xFF6F3D86, //  4: Purple
    0xFF588D43, //  5: Green
    0xFF352879, //  6: Blue
    0xFFB8C76F, //  7: Yellow
    0xFF6F4F25, //  8: Orange
    0xFF433900, //  9: Brown
    0xFF9A6759, // 10: Light red
    0xFF444444, // 11: Dark grey
    0xFF6C6C6C, // 12: Medium grey
    0xFF9AD284, // 13: Light green
    0xFF6C5EB5, // 14: Light blue
    0xFF959595  // 15: Light grey
};

void VBC_NewFrame::Execute()
{
    ViceFrame frame;
    frame.m_width = m_width;
    frame.m_height = m_lastLine - m_firstLine;
    frame.m_pixels = new u32[m_width * m_height];
    u32* out = frame.m_pixels;
    u8* in = m_buffer + m_firstLine * m_width;
    for (int p = 0; p < (frame.m_width * frame.m_height); p++)
    {
        *out++ = c64Palette[*in++ & 15];
    }
    ViceBridge::Instance().QueueFrame(frame);
    delete m_buffer;
}


extern "C" void helper_autostart_prg(const char* path);
extern "C" int helper_set_breakpoint(int memaddr);
extern "C" void mon_breakpoint_delete_checkpoint(int checkpointId);

void VBC_RunPrg::Execute()
{
    ViceBridge::Instance().ClearViceStopped();
    helper_autostart_prg(m_path.c_str());
}

void VBC_SetBreakpoint::Execute()
{
    CPUBreakpoint bp;
    bp.m_id = m_breakpointID;
    bp.m_addr = m_addr;
    bp.m_fileID = m_fileID;
    bp.m_lineID = m_lineID;
    bp.m_once = m_oneShot;
    s_emulationState.m_breakpoints.push_back(bp);
}

void VBC_ClearBreakpoint::Execute()
{
    mon_breakpoint_delete_checkpoint(m_breakpointID);
}

void ViceBridge::ClearBreakpoint(int breakpointID)
{
    auto cmd = new VBC_ClearBreakpoint;
    cmd->m_breakpointID = breakpointID;
    SendMad2Vice(cmd);
}
