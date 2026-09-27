#include "common.h"
#include "ViceBridge.h"
#include "LogManager.h"

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
        *out = c64Palette[*in & 15];
    }
    gViceBridge->QueueFrame(frame);
    delete m_buffer;
}
