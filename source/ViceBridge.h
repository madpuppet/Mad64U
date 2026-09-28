#pragma once
#include <semaphore>
#include <deque>

// Bridge between VICE and MAD64
//
// Currently:
//  - send new frames from vice (do heavy work process on thread), then queue for MAD64 to pop
//
// TODO
//  - send keyboard/joystick from MAD64 to VICE
//  - send "run prg" from MAD64 to VICE
//  - send "run D64" from MAD64 to VICE
//  - send toggle breakpoint from MAD64 to VICE
//  - send control cmds (Break, SingleStep, Continue, Break At RasterBeam)
//  - send status from VICE to MAD64 (sprites, registers, memory, rasterPos)

struct ViceBridgeCmd
{
    virtual void Execute() = 0;
};


struct ViceFrame
{
    u32* m_pixels;
    int m_width;
    int m_height;
};

struct VBC_NewFrame : ViceBridgeCmd
{
    virtual void Execute() override;
    u8* m_buffer;
    int m_width;
    int m_height;
};

struct VBC_RunPrg : ViceBridgeCmd
{
    virtual void Execute() override;
    std::string m_path;
};

struct VBC_SetBreakpoint : ViceBridgeCmd
{
    virtual void Execute() override;
    int m_fileID;
    int m_lineID;
    u32 m_addr;
};

struct VBC_ClearBreakpoint : ViceBridgeCmd
{
    virtual void Execute() override;
    int m_breakpointID;
};

struct VBC_BreakpointSet : ViceBridgeCmd
{
    virtual void Execute() override;
    int m_fileID;
    int m_lineID;
    int m_breakpointID;
};

struct VBC_BreakpointHit : ViceBridgeCmd
{
    virtual void Execute() override;
};

struct VBC_Continue : ViceBridgeCmd
{
    virtual void Execute() override;
    bool m_singleStep;
};

struct VBC_UpdateViceState : ViceBridgeCmd
{
    virtual void Execute() override;
    int m_rasterline;
    int m_rasterCycle;
    int m_pc;
    int m_acc;
    int m_x;
    int m_y;
    int m_flags;
};

struct ViceState
{
    int m_rasterLine;
    int m_rasterCycle;
    int m_pc;
    int m_acc;
    int m_x;
    int m_y;
    int m_flags;
};

class ViceBridge
{
public:
    void Start();

    void Queue(ViceBridgeCmd* cmd)
    {
        m_cmdMutex.lock();
        m_cmds.push_back(cmd);
        m_cmdMutex.unlock();
        m_signalMsg.release();
    }

    void QueueFrame(ViceFrame frame)
    {
        m_cmdMutex.lock();
        m_frames.emplace_back(frame);
        m_cmdMutex.unlock();
    }

    bool PopFrame(ViceFrame &frame)
    {
        m_cmdMutex.lock();
        if (m_frames.size() > 0)
        {
            frame = m_frames.front();
            m_frames.pop_front();
            m_cmdMutex.unlock();
            return true;
        }
        m_cmdMutex.unlock();
        return false;
    }

    void SendMad2Vice(ViceBridgeCmd* cmd)
    {
        m_mutexMad2Vice.lock();
        m_syncMad2Vice.push_back(cmd);
        m_mutexMad2Vice.unlock();
    }
    ViceBridgeCmd* PopMad2Vice()
    {
        ViceBridgeCmd* cmd = nullptr;
        m_mutexMad2Vice.lock();
        if (!m_syncMad2Vice.empty())
        {
            cmd = m_syncMad2Vice.front();
            m_syncMad2Vice.pop_front();
        }
        m_mutexMad2Vice.unlock();
        return nullptr;
    }
    void ExecuteViceCmds();

    void SendVice2Mad(ViceBridgeCmd* cmd)
    {
        m_mutexVice2Mad.lock();
        m_syncVice2Mad.push_back(cmd);
        m_mutexVice2Mad.unlock();
    }
    ViceBridgeCmd* PopVice2Mad()
    {
        ViceBridgeCmd* cmd = nullptr;
        m_mutexVice2Mad.lock();
        if (!m_syncVice2Mad.empty())
        {
            cmd = m_syncVice2Mad.front();
            m_syncVice2Mad.pop_front();
        }
        m_mutexVice2Mad.unlock();
        return nullptr;
    }
    void ExecuteMadCmds();

    // breakpoint was hit...
    void BreakpointHit();

    // HELPERS
    void ClearBreakpoint(int breakpointID)
    {
        auto cmd = new VBC_ClearBreakpoint;
        cmd->m_breakpointID = breakpointID;
        SendMad2Vice(cmd);
    }

    bool HasViceStopped() { return m_vice_stopped; }
    void ClearViceStopped() { m_vice_stopped = false; }

    void SingleStep();
    void Continue();

    // general state info
    ViceState& GetViceState() { return m_viceState; }

    void SetActiveFileID(int fileID) { m_activeFileID = fileID; }
    int GetActiveFileID() { return m_activeFileID; }

protected:
    int m_activeFileID = 0;
    ViceState m_viceState;

    // data blocks from VICE to MAD64
    // used for sound and frame
    std::counting_semaphore<256> m_signalMsg{ 0 };
    std::mutex m_cmdMutex;
    std::deque<ViceBridgeCmd*> m_cmds;
    std::deque<ViceFrame> m_frames;

    // syncing
    std::mutex m_mutexMad2Vice;
    std::deque<ViceBridgeCmd*> m_syncMad2Vice;

    std::mutex m_mutexVice2Mad;
    std::deque<ViceBridgeCmd*> m_syncVice2Mad;

    bool m_vice_stopped = false;
};
extern ViceBridge* gViceBridge;


