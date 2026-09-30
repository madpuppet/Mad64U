#include "common.h"
#include "ViceBridge.h"
#include "LogManager.h"
#include "SourceFileManager.h"
#include "WindowManager.h"

ViceBridge* gViceBridge = nullptr;

void ViceBridge::Start()
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

extern "C" void mad64_video_refresh(u8* buffer, int width, int height)
{
    auto cmd = new VBC_NewFrame;
    cmd->m_buffer = new u8[width * height];
    cmd->m_height = height;
    cmd->m_width = width;
    memcpy(cmd->m_buffer, buffer, width * height);
    gViceBridge->Queue(cmd);
}

extern "C" void mad64_process_vice_commands(void)
{
    gViceBridge->ExecuteViceCmds();
}

extern "C" void mad64_breakpoint_hit(void)
{
    gViceBridge->BreakpointHit();
}

extern "C" void mon_instructions_step(int);
extern "C" void mon_go();
extern "C" void monitor_startup_trap();

extern "C" void mad64_update_vice_state(int rasterline, int rasterCycle, int pc, int acc, int x, int y, int flags)
{
    auto cmd = new VBC_UpdateViceState;
    cmd->m_acc = acc;
    cmd->m_flags = flags;
    cmd->m_pc = pc;
    cmd->m_rasterCycle = rasterCycle;
    cmd->m_rasterline = rasterline;
    cmd->m_x = x;
    cmd->m_y = y;
    gViceBridge->SendVice2Mad(cmd);
}

void VBC_UpdateViceState::Execute()
{
    auto& state = gViceBridge->GetViceState();
    state.m_acc = m_acc;
    state.m_flags = m_flags;
    state.m_pc = m_pc;
    state.m_rasterCycle = m_rasterCycle;
    state.m_rasterLine = m_rasterline;
    state.m_x = m_x;
    state.m_y = m_y;
}

void ViceBridge::BreakpointHit()
{
    m_vice_stopped = true;
    SendVice2Mad(new VBC_BreakPointHit);

    while (m_vice_stopped)
    {
        Sleep(1);
        ExecuteViceCmds();
    }
}

void VBC_BreakPointHit::Execute()
{
    WindowMessageStruct msgBH;
    msgBH.m_type = WindowMessage::Window_BreakpointHit;
    msgBH.m_flags = WMF_EarlyOut | WMF_Window | WMF_TabActive;
    msgBH.m_sourceFile = SourceFileManager::Instance().FindFileByID(gViceBridge->GetActiveFileID());
    if (msgBH.m_sourceFile)
    {
        WindowManager::Instance().Message(msgBH);
    }
}

void ViceBridge::SingleStep()
{
    auto cmd = new VBC_Continue;
    cmd->m_singleStep = true;
    SendMad2Vice(cmd);
}

void ViceBridge::Continue()
{
    auto cmd = new VBC_Continue;
    cmd->m_singleStep = false;
    SendMad2Vice(cmd);
}

void ViceBridge::Pause()
{
    SendMad2Vice(new VBC_Pause);
}

void VBC_Continue::Execute()
{
    gViceBridge->ClearViceStopped();
    if (m_singleStep)
        mon_instructions_step(1);
    else
        mon_go();
}

void VBC_Pause::Execute()
{
    monitor_startup_trap();
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
    frame.m_height = m_height;
    frame.m_pixels = new u32[m_width * m_height];
    u32* out = frame.m_pixels;
    u8* in = m_buffer;
    for (int p = 0; p < (m_width * m_height); p++)
    {
        *out++ = c64Palette[*in++ & 15];
    }
    gViceBridge->QueueFrame(frame);
    delete m_buffer;
}


extern "C" void helper_autostart_prg(const char* path);
extern "C" int helper_set_breakpoint(int memaddr);
extern "C" void mon_breakpoint_delete_checkpoint(int checkpointId);

void VBC_RunPrg::Execute()
{
    if (gViceBridge->HasViceStopped())
    {
        gViceBridge->ClearViceStopped();
        mon_go();
    }

    helper_autostart_prg(m_path.c_str());
}

void VBC_SetBreakpoint::Execute()
{
    int id = helper_set_breakpoint(m_addr);
    auto cmd = new VBC_BreakpointSet;
    cmd->m_breakpointID = id;
    cmd->m_fileID = m_fileID;
    cmd->m_lineID = m_lineID;
    cmd->m_addr = m_addr;
    gViceBridge->SendVice2Mad(cmd);
}

void VBC_ClearBreakpoint::Execute()
{
    mon_breakpoint_delete_checkpoint(m_breakpointID);
}

void VBC_BreakpointSet::Execute()
{
    SourceFileManager::Instance().OnBreakpointSet(m_fileID, m_lineID, m_breakpointID, m_addr);
}
