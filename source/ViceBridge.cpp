#include "common.h"
#include "ViceBridge.h"
#include "LogManager.h"
#include "SourceFileManager.h"
#include "WindowManager.h"

extern "C" int helper_set_video_mode(int pal);
extern "C" int helper_set_volume(int volume);
extern "C" uint64_t helper_get_clock_cycle();
extern "C" void helper_get_raster_pos(unsigned int* rasterLine, unsigned int* rasterCycle);
extern "C" int helper_suspend_sound();
extern "C" int helper_resume_sound();
extern "C" void helper_keyboard_set(int row, int column, int value);
extern "C" void helper_autostart_prg(const char* path);
extern "C" void helper_autostart_d64(const char* path);
extern "C" void helper_autostart_crt(const char* path);
extern "C" int helper_set_breakpoint(int memaddr);
extern "C" void helper_mount_d64(const char* path);
extern "C" void helper_set_warp_mode(bool enable);
extern "C" void helper_get_vicii_regs(unsigned char* written, unsigned char* read, int count);
extern "C" void helper_set_sid_model(int is8580);
extern "C" void helper_get_sid_state(unsigned char* regs, unsigned int* accumulator, unsigned int* shiftRegister, unsigned char* envelope, unsigned char* envelopeState, int* model);
extern "C" void helper_get_vicii_beam(int* line, int* beamX, int* cyclesPerLine, int* screenHeight);
extern "C" void helper_get_vicii_memory(unsigned char* ram, unsigned char* colourRam, unsigned char* charRom, int* vbank);
extern "C" void helper_get_cpu_memory(unsigned char* mem);
extern "C" void helper_set_joystick(int port, int value);
extern "C" void helper_cold_reset();
extern "C" int helper_get_input_state(unsigned char* keyMatrix, unsigned char* joystick, unsigned char* potx, unsigned char* poty, char* device, int deviceLen);

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
    bool m_isMemBP = false;
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

    // the raster point: beam position within the frame, in cycles (line * cyclesPerLine + cycle)
    VicRasterPointMode m_rasterMode = VicRasterPointMode::Off;
    int m_rasterPos = 0;
    int m_rasterLastPos = 0;

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

    void ClearBreakpoint(int id)
    {
        std::erase_if(m_breakpoints, [id](const CPUBreakpoint& bp)
            {
                return bp.m_id == id;
            });
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

void CheckBreakpoints(u16 addr, bool memBP)
{
    // check any breakpoints...
    for (auto bp : s_emulationState.m_breakpoints)
    {
        if (bp.m_addr == addr && memBP == bp.m_isMemBP)
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

// beam position within the frame in cycles, and the number of cycles in a frame
static int GetBeamPos(int& frameCycles)
{
    int line = 0, beamX = 0, cyclesPerLine = 63, screenHeight = 312;
    helper_get_vicii_beam(&line, &beamX, &cyclesPerLine, &screenHeight);
    frameCycles = cyclesPerLine * screenHeight;
    return line * cyclesPerLine + beamX / 8;
}

// Called once per instruction while the raster point is in use.
// If the beam reached or passed the point since the last instruction, either captures a VIC snapshot
// (TopOfFrame / Refresh) or returns true to stop the emulator (Break).
static bool CheckRasterPoint()
{
    auto& es = s_emulationState;
    int frameCycles;
    int pos = GetBeamPos(frameCycles);
    int last = es.m_rasterLastPos;
    int target = es.m_rasterPos;
    es.m_rasterLastPos = pos;

    bool crossed;
    if (pos >= last)
        crossed = target > last && target <= pos;
    else
        crossed = target > last || target <= pos;       // beam wrapped into the next frame

    if (!crossed)
        return false;

    if (es.m_rasterMode == VicRasterPointMode::Break)
        return true;

    ViceBridge::Instance().CaptureVicSnapshot();
    return false;
}

extern "C" void mad64_update_ram(u8 *ram)
{
    memcpy(s_emulationState.m_ram, ram, 65536);
}

extern "C" void mad64_track_ram_store(u8* ram, u64 clock, u16 addr)
{
    s_emulationState.m_heatMap[addr] = clock;
    s_emulationState.m_ram[addr] = ram[addr];
    CheckBreakpoints(addr, true);
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

    bool rasterHit = false;
    if (s_emulationState.m_rasterMode != VicRasterPointMode::Off)
        rasterHit = CheckRasterPoint();

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
    else if (rasterHit)
    {
        auto& vb = ViceBridge::Instance();

        vb.SetViceStopped();
        vb.SendVice2Mad(new VBC_BreakPointHit);

        // busy wait till we get a cmd from the editor to run some more emulation
        vb.ProcessViceCmdsTillContinue();
    }
    else
        CheckBreakpoints(pc_addr, false);
}

int ViceBridge::SetBreakpoint(int fileID, int lineID, u16 addr, bool oneShot, bool memBP)
{
    int breakpointID = m_nextBreakpointID++;
    auto cmd = new VBC_SetBreakpoint;
    cmd->m_fileID = fileID;
    cmd->m_lineID = lineID;
    cmd->m_addr = addr;
    cmd->m_breakpointID = breakpointID;
    cmd->m_oneShot = oneShot;
    cmd->m_memBP = memBP;
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

    // called once per frame on the emulation thread
    ViceBridge::Instance().CaptureSidState();
    ViceBridge::Instance().CaptureDriveState();
    ViceBridge::Instance().CaptureInputState();
}

static SidScope s_sidScope;

extern "C" void mad64_sid_voice_output(int cycles, int v0, int v1, int v2)
{
    if (cycles <= 0)
        return;

    // the outputs are held for 'cycles' cycles; emit one sample for every SID_SCOPE_DECIMATE cycles that pass
    auto& scope = s_sidScope;
    u32 total = scope.m_cycleAcc + (u32)cycles;
    u32 count = total / SID_SCOPE_DECIMATE;
    scope.m_cycleAcc = total % SID_SCOPE_DECIMATE;
    if (count == 0)
        return;
    if (count > SID_SCOPE_SAMPLES)
        count = SID_SCOPE_SAMPLES;

    i16 s0 = (i16)std::clamp(v0, -32768, 32767);
    i16 s1 = (i16)std::clamp(v1, -32768, 32767);
    i16 s2 = (i16)std::clamp(v2, -32768, 32767);

    u32 idx = scope.m_writeIdx.load(std::memory_order_relaxed);
    for (u32 i = 0; i < count; i++)
    {
        u32 r = (idx + i) & (SID_SCOPE_SAMPLES - 1);
        scope.m_samples[0][r] = s0;
        scope.m_samples[1][r] = s1;
        scope.m_samples[2][r] = s2;
    }
    scope.m_writeIdx.store(idx + count, std::memory_order_release);
}

const SidScope& ViceBridge::GetSidScope()
{
    return s_sidScope;
}

//==================================================================================================================================
// Disk drive activity
//
// Accesses come from two places:
//  - the disk image layer reports every sector read/written through it (virtual drives, image writeback)
//  - with true drive emulation, once per frame we look at the drive mechanics (motor, head, write mode and the sector
//    header under the head) and the DOS job queue in drive RAM
// Everything is kept in s_driveState on the emulation thread and copied out for the UI.
//==================================================================================================================================
static DriveState s_driveState;
static u8 s_prevJobCode[MAD_DRIVE_UNITS][MAD_DRIVE_MAX_JOBS];

static void RecordDriveAccess(int unit, int track, int sector, DriveAccess kind, bool logEvent)
{
    if (unit < 0 || unit >= MAD_DRIVE_UNITS || track < 1 || track > DRIVE_MAP_TRACKS || sector < 0 || sector >= DRIVE_MAP_SECTORS)
        return;

    auto& u = s_driveState.m_units[unit];
    u32 frame = s_driveState.m_frame;
    if (kind == DriveAccess::Write)
        u.m_lastWrite[track][sector] = frame;
    else
        u.m_lastRead[track][sector] = frame;
    u.m_lastActivityFrame = frame;

    if (logEvent)
    {
        // repeated access to the same sector just refreshes the newest event
        if (u.m_eventCount > 0)
        {
            auto& last = u.m_events[(u.m_eventCount - 1) % DRIVE_EVENT_COUNT];
            if (last.m_track == track && last.m_sector == sector && last.m_kind == kind)
            {
                last.m_frame = frame;
                return;
            }
        }
        auto& ev = u.m_events[u.m_eventCount % DRIVE_EVENT_COUNT];
        ev.m_track = (u8)track;
        ev.m_sector = (u8)sector;
        ev.m_kind = kind;
        ev.m_frame = frame;
        u.m_eventCount++;
    }
}

extern "C" void mad64_disk_sector_access(const void* image, unsigned int track, unsigned int sector, int isWrite)
{
    int unit = helper_disk_image_unit(image);
    RecordDriveAccess(unit, (int)track, (int)sector, isWrite ? DriveAccess::Write : DriveAccess::Read, true);
}

void ViceBridge::CaptureDriveState()
{
    auto& ds = s_driveState;
    bool changed = false;

    for (int unit = 0; unit < MAD_DRIVE_UNITS; unit++)
    {
        auto& u = ds.m_units[unit];
        mad_drive_info_t info;
        helper_get_drive_info(unit, &info);
        if (memcmp(&info, &u.m_info, sizeof(info)) != 0)
            changed = true;
        u.m_info = info;

        if (!info.trueEmulation)
            continue;

        // mechanics: the sector passing under the head, and anything being written to it
        if (info.motorOn && info.headerTrack >= 1 && info.headerTrack <= DRIVE_MAP_TRACKS && info.headerSector >= 0 && info.headerSector < DRIVE_MAP_SECTORS)
        {
            u.m_lastHead[info.headerTrack][info.headerSector] = ds.m_frame;
            if (info.writeMode)
                RecordDriveAccess(unit, info.headerTrack, info.headerSector, DriveAccess::Write, true);
        }

        // DOS job queue: codes with bit 7 set are pending jobs
        for (int j = 0; j < info.jobCount; j++)
        {
            u8 code = info.jobCode[j];
            bool isNew = code != s_prevJobCode[unit][j];
            s_prevJobCode[unit][j] = code;
            if (!(code & 0x80))
                continue;

            switch (code & 0xf0)
            {
                case 0x80: RecordDriveAccess(unit, info.jobTrack[j], info.jobSector[j], DriveAccess::Read, isNew); break;
                case 0x90: RecordDriveAccess(unit, info.jobTrack[j], info.jobSector[j], DriveAccess::Write, isNew); break;
                case 0xa0: RecordDriveAccess(unit, info.jobTrack[j], info.jobSector[j], DriveAccess::Verify, isNew); break;
                default: break;
            }
        }

        if (info.motorOn)
            u.m_lastActivityFrame = ds.m_frame;
    }

    // only publish while something is changing or still fading out
    for (auto& u : ds.m_units)
    {
        if (u.m_lastActivityFrame != 0 && ds.m_frame - u.m_lastActivityFrame <= DRIVE_FADE_FRAMES)
            changed = true;
    }

    if (changed || m_driveStateVersion == 0)
    {
        std::lock_guard<std::mutex> lock(m_driveStateMutex);
        m_driveState = ds;
        m_driveStateVersion++;
    }
    ds.m_frame++;
}

bool ViceBridge::GetDriveState(DriveState& state, u32& version)
{
    std::lock_guard<std::mutex> lock(m_driveStateMutex);
    if (version == m_driveStateVersion)
        return false;
    state = m_driveState;
    version = m_driveStateVersion;
    return true;
}

void ViceBridge::CaptureSidState()
{
    SidState state;
    helper_get_sid_state(state.m_regs, state.m_accumulator, state.m_shiftRegister, state.m_envelope, state.m_envelopeState, &state.m_model);

    std::lock_guard<std::mutex> lock(m_sidStateMutex);
    m_sidState = state;
    m_sidStateVersion++;
}

bool ViceBridge::GetSidState(SidState& state, u32& version)
{
    std::lock_guard<std::mutex> lock(m_sidStateMutex);
    if (version == m_sidStateVersion)
        return false;
    state = m_sidState;
    version = m_sidStateVersion;
    return true;
}

void ViceBridge::CaptureInputState()
{
    InputState state;
    state.m_potPortMask = helper_get_input_state(state.m_keyMatrix, state.m_joystick, state.m_potX, state.m_potY, &state.m_device[0][0], INPUT_DEVICE_NAME_LEN);

    std::lock_guard<std::mutex> lock(m_inputStateMutex);
    if (state == m_inputState)
        return;
    m_inputState = state;
    m_inputStateVersion++;
}

bool ViceBridge::GetInputState(InputState& state, u32& version)
{
    std::lock_guard<std::mutex> lock(m_inputStateMutex);
    if (version == m_inputStateVersion)
        return false;
    state = m_inputState;
    version = m_inputStateVersion;
    return true;
}

static uint64_t s_clock_start = 0;
static uint64_t s_clock_elapsed = 0;

void ViceBridge::ProcessViceCmdsTillContinue()
{
    while (m_vice_stopped)
    {
        Sleep(1);
        ExecuteViceCmds();

        // keys and joysticks still change while stopped
        CaptureInputState();
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
    state.m_rasterCycle = tp.cycle;
    state.m_rasterLine = tp.raster;
    state.m_clock = tp.clock;
    return state;
}

u8* ViceBridge::GetRam()
{
    return s_emulationState.m_ram;
}

u8* ViceBridge::GetCpuMemory()
{
    helper_get_cpu_memory(m_cpuMemory);
    return m_cpuMemory;
}

u64* ViceBridge::GetPCTouchRam()
{
    return s_emulationState.m_pcTouch;
}

u64* ViceBridge::GetHeatMap()
{
    return s_emulationState.m_heatMap;
}

void ViceBridge::GetVicRasterState(VicRasterState& state)
{
    int line = 0, liveCount = 0, cyclesPerLine = 63, screenHeight = 312;
    helper_get_vicii_beam(&line, &liveCount, &cyclesPerLine, &screenHeight);

    // the VICE thread may be mid-update, so sanitise everything
    state.m_cyclesPerLine = std::clamp(cyclesPerLine, 1, VIC_RASTER_MAX_WIDTH / 8);
    state.m_lineCount = std::clamp(screenHeight, 1, VIC_RASTER_MAX_LINES);
    state.m_line = std::clamp(line, 0, state.m_lineCount - 1);
    state.m_liveCount = std::clamp(liveCount, 0, state.Width());
    helper_get_vicii_regs(state.m_regs, state.m_regsRead, 0x40);

    int bank = 0;
    helper_get_vicii_memory(state.m_ram, state.m_colourRam, state.m_charRom, &bank);
    state.m_bank = bank & 0xc000;
}

void ViceBridge::BreakpointHit(const CPUBreakpoint &bp)
{
    auto cmd = new VBC_BreakPointHit;
    cmd->m_breakpointID = bp.m_id;
    cmd->m_fileID = bp.m_fileID;
    cmd->m_lineID = bp.m_lineID;
    SendVice2Mad(cmd);
}

void VBC_SetWarpMode::Execute()
{
    helper_set_warp_mode(m_enable);
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
    cmd->m_steps = 64;
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

void ViceBridge::ColdReboot()
{
    SendMad2Vice(new VBC_ColdReset);
    Continue();
}

void VBC_ColdReset::Execute()
{
    helper_cold_reset();
}

void VBC_Continue::Execute()
{
    ViceBridge::Instance().ClearViceStopped();
    s_emulationState.m_runCycles = m_steps;
}

void ViceBridge::SetRasterPoint(VicRasterPointMode mode, int line, int cycle)
{
    auto cmd = new VBC_SetRasterPoint;
    cmd->m_mode = mode;
    cmd->m_line = line;
    cmd->m_cycle = cycle;
    SendMad2Vice(cmd);
}

void VBC_SetRasterPoint::Execute()
{
    auto& es = s_emulationState;
    int cyclesPerLine = 63, line = 0, beamX = 0, screenHeight = 312;
    helper_get_vicii_beam(&line, &beamX, &cyclesPerLine, &screenHeight);
    int frameCycles = (std::max)(cyclesPerLine * screenHeight, 1);

    es.m_rasterMode = m_mode;
    if (m_mode == VicRasterPointMode::Break || m_mode == VicRasterPointMode::Refresh)
        es.m_rasterPos = (m_line * cyclesPerLine + m_cycle) % frameCycles;
    else
        es.m_rasterPos = 0;

    // if the beam is already exactly there, the point triggers when it comes round again next frame
    es.m_rasterLastPos = line * cyclesPerLine + beamX / 8;
}

void ViceBridge::CaptureVicSnapshot()
{
    std::lock_guard<std::mutex> lock(m_vicSnapshotMutex);
    GetVicRasterState(m_vicSnapshot);
    m_vicSnapshotVersion++;
}

bool ViceBridge::GetVicSnapshot(VicRasterState& state, u32& version)
{
    std::lock_guard<std::mutex> lock(m_vicSnapshotMutex);
    if (version == m_vicSnapshotVersion)
        return false;
    state = m_vicSnapshot;
    version = m_vicSnapshotVersion;
    return true;
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

void VBC_SetVolume::Execute()
{
    helper_set_volume(m_volume);
}

void VBC_Joystick::Execute()
{
    helper_set_joystick(m_port, m_value);
}

//==================================================================================================================================
// Host controllers
//==================================================================================================================================

#define JOY_UP    0x01
#define JOY_DOWN  0x02
#define JOY_LEFT  0x04
#define JOY_RIGHT 0x08
#define JOY_FIRE  0x10

// stick deflection needed to count as a direction (axes are -32768..32767)
#define JOY_DEADZONE 12000

static u8 AxesToJoystick(int x, int y)
{
    u8 value = 0;
    if (x < -JOY_DEADZONE) value |= JOY_LEFT;
    if (x > JOY_DEADZONE) value |= JOY_RIGHT;
    if (y < -JOY_DEADZONE) value |= JOY_UP;
    if (y > JOY_DEADZONE) value |= JOY_DOWN;
    return value;
}

bool ViceBridge::HandleControllerEvent(SDL_Event* e)
{
    auto find = [this](SDL_JoystickID id)
        {
            return std::find_if(m_controllers.begin(), m_controllers.end(), [id](const Controller& c) { return c.m_id == id; });
        };

    switch (e->type)
    {
        case SDL_EVENT_GAMEPAD_ADDED:
        {
            if (find(e->gdevice.which) != m_controllers.end())
                return true;
            Controller c;
            c.m_id = e->gdevice.which;
            c.m_gamepad = SDL_OpenGamepad(c.m_id);
            if (!c.m_gamepad)
            {
                Log(LogGroup::System, "Gamepad open failed: {}", SDL_GetError());
                return true;
            }
            m_controllers.push_back(c);
            Log(LogGroup::System, "Gamepad connected: {}", SDL_GetGamepadName(c.m_gamepad));
            break;
        }

        case SDL_EVENT_JOYSTICK_ADDED:
        {
            // devices with a gamepad mapping arrive as SDL_EVENT_GAMEPAD_ADDED too
            if (SDL_IsGamepad(e->jdevice.which) || find(e->jdevice.which) != m_controllers.end())
                return true;
            Controller c;
            c.m_id = e->jdevice.which;
            c.m_joystick = SDL_OpenJoystick(c.m_id);
            if (!c.m_joystick)
            {
                Log(LogGroup::System, "Joystick open failed: {}", SDL_GetError());
                return true;
            }
            m_controllers.push_back(c);
            Log(LogGroup::System, "Joystick connected: {}", SDL_GetJoystickName(c.m_joystick));
            break;
        }

        case SDL_EVENT_GAMEPAD_REMOVED:
        case SDL_EVENT_JOYSTICK_REMOVED:
        {
            SDL_JoystickID id = (e->type == SDL_EVENT_GAMEPAD_REMOVED) ? e->gdevice.which : e->jdevice.which;
            auto it = find(id);
            if (it == m_controllers.end())
                return true;
            if (it->m_gamepad)
                SDL_CloseGamepad(it->m_gamepad);
            if (it->m_joystick)
                SDL_CloseJoystick(it->m_joystick);
            m_controllers.erase(it);
            Log(LogGroup::System, "Controller disconnected");
            break;
        }

        case SDL_EVENT_GAMEPAD_AXIS_MOTION:
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        case SDL_EVENT_GAMEPAD_BUTTON_UP:
        case SDL_EVENT_GAMEPAD_REMAPPED:
        case SDL_EVENT_JOYSTICK_AXIS_MOTION:
        case SDL_EVENT_JOYSTICK_HAT_MOTION:
        case SDL_EVENT_JOYSTICK_BUTTON_DOWN:
        case SDL_EVENT_JOYSTICK_BUTTON_UP:
            break;

        default:
            return false;
    }

    SendJoystickState();
    return true;
}

void ViceBridge::SetJoystickPort(int port)
{
    m_joystickPort = (port == 1) ? 1 : 2;
    SendJoystickState();
}

// recompute both ports from the current state of every controller and send VICE whatever changed
void ViceBridge::SendJoystickState()
{
    u8 ports[2] = {};
    for (size_t i = 0; i < m_controllers.size(); i++)
    {
        auto& c = m_controllers[i];
        u8 value = 0;
        if (c.m_gamepad)
        {
            auto pad = c.m_gamepad;
            value = AxesToJoystick(SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTX), SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTY));
            if (SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_UP)) value |= JOY_UP;
            if (SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_DOWN)) value |= JOY_DOWN;
            if (SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_LEFT)) value |= JOY_LEFT;
            if (SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT)) value |= JOY_RIGHT;
            // A fires, B pushes up (jump in most platformers)
            if (SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_SOUTH))
                value |= JOY_FIRE;
            if (SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_EAST))
                value |= JOY_UP;
        }
        else if (c.m_joystick)
        {
            auto joy = c.m_joystick;
            if (SDL_GetNumJoystickAxes(joy) >= 2)
                value = AxesToJoystick(SDL_GetJoystickAxis(joy, 0), SDL_GetJoystickAxis(joy, 1));
            if (SDL_GetNumJoystickHats(joy) >= 1)
            {
                u8 hat = SDL_GetJoystickHat(joy, 0);
                if (hat & SDL_HAT_UP) value |= JOY_UP;
                if (hat & SDL_HAT_DOWN) value |= JOY_DOWN;
                if (hat & SDL_HAT_LEFT) value |= JOY_LEFT;
                if (hat & SDL_HAT_RIGHT) value |= JOY_RIGHT;
            }
            // button 0 fires, button 1 pushes up
            int buttons = SDL_GetNumJoystickButtons(joy);
            if (buttons >= 1 && SDL_GetJoystickButton(joy, 0))
                value |= JOY_FIRE;
            if (buttons >= 2 && SDL_GetJoystickButton(joy, 1))
                value |= JOY_UP;
        }

        // first controller drives the selected port, second the other one, and so on alternating
        int port = (i & 1) ? 3 - m_joystickPort : m_joystickPort;
        ports[port - 1] |= value;
    }

    for (int p = 0; p < 2; p++)
    {
        if (ports[p] == m_sentJoystick[p])
            continue;
        m_sentJoystick[p] = ports[p];
        auto cmd = new VBC_Joystick;
        cmd->m_port = p + 1;
        cmd->m_value = ports[p];
        SendMad2Vice(cmd);
    }
}

void VBC_SetSidModel::Execute()
{
    helper_set_sid_model(m_is8580 ? 1 : 0);
}

void ViceBridge::SetViceStopped()
{
    m_vice_stopped = true;
    CaptureSidState();
    CaptureDriveState();
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

void VBC_MountDisk::Execute()
{
    helper_mount_d64(m_path.c_str());
}

void VBC_RunPrg::Execute()
{
    ViceBridge::Instance().ClearViceStopped();
    std::filesystem::path path = m_path;
    std::filesystem::path ext = path.extension();
    if (ext == ".d64" || ext == ".D64")
    {
        helper_autostart_d64(m_path.c_str());
    }
    else if (ext == ".crt" || ext == ".CRT")
    {
        helper_autostart_crt(m_path.c_str());
    }
    else
    {
        helper_autostart_prg(m_path.c_str());
    }
}

void VBC_SetBreakpoint::Execute()
{
    CPUBreakpoint bp;
    bp.m_id = m_breakpointID;
    bp.m_addr = m_addr;
    bp.m_fileID = m_fileID;
    bp.m_lineID = m_lineID;
    bp.m_once = m_oneShot;
    bp.m_isMemBP = m_memBP;
    s_emulationState.m_breakpoints.push_back(bp);
}

void VBC_ClearBreakpoint::Execute()
{
    s_emulationState.ClearBreakpoint(m_breakpointID);
}

void ViceBridge::ClearBreakpoint(int breakpointID)
{
    auto cmd = new VBC_ClearBreakpoint;
    cmd->m_breakpointID = breakpointID;
    SendMad2Vice(cmd);
}

void VBC_Key::Execute()
{
    if (m_pressed)
    {
        if (m_r2 != -1)
            helper_keyboard_set(m_r2, m_c2, 1);
        helper_keyboard_set(m_row, m_column, 1);
    }
    else
    {
        helper_keyboard_set(m_row, m_column, 0);
        if (m_r2 != -1)
            helper_keyboard_set(m_r2, m_c2, 0);
    }
    if (m_cancelShift)
    {
        helper_keyboard_set(1, 7, 0);
        helper_keyboard_set(6, 4, 0);
    }
}


