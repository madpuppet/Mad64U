#pragma once
#include <semaphore>
#include <deque>

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

protected:
    std::counting_semaphore<256> m_signalMsg{ 0 };
    std::mutex m_cmdMutex;
    std::deque<ViceBridgeCmd*> m_cmds;
    std::deque<ViceFrame> m_frames;
};
extern ViceBridge* gViceBridge;


