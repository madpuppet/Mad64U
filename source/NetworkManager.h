#pragma once

#include "Singleton.h"
#include <semaphore>
#include <map>

enum class NetworkMessage
{
    SetIP,
    Command
};

struct NetworkResult
{
    std::string connection;
    std::string contentType;
    u8* mem = nullptr;
    size_t size = 0;
    size_t amountRecieved = 0;
};

struct NetworkMessageStruct
{
    NetworkMessageStruct(NetworkMessage msgType) : m_msgType(msgType) {}
    NetworkMessage m_msgType;
};

struct NMS_SetIP : public NetworkMessageStruct
{
    NMS_SetIP(const std::string &ip) : NetworkMessageStruct(NetworkMessage::SetIP), m_ip(ip) {}

    std::string m_ip;
};

struct NMS_Command : public NetworkMessageStruct
{
    NMS_Command(const std::string& command, u8 *mem=nullptr, size_t size=0, const std::string &filename="", bool isGet = false)
        : NetworkMessageStruct(NetworkMessage::Command), m_command(command), m_contentMem(mem), m_filename(filename), m_contentSize(size), m_isGet(isGet) {}

    std::string m_command;
    std::string m_filename;
    bool m_isGet = false;
    u8* m_contentMem = nullptr;
    size_t m_contentSize = 0;
};

#define NETWORK_MESSAGE_LIST_SIZE 256

struct NetworkStatus
{
    bool m_connected;
    std::string m_ipAddress;
    std::string m_hostName;
};

class NetworkManager : public Singleton<NetworkManager>
{
public:
    NetworkManager();
    ~NetworkManager();
    
    void Message(NetworkMessageStruct *msg);
    void GetNetworkStatus(NetworkStatus& status);

    // helpers
    void SendReset() { Message(new NMS_Command("machine:reset")); }
    void SendPause() { Message(new NMS_Command("machine:pause")); }
    void SendResume() { Message(new NMS_Command("machine:resume")); }
    void SendPowerOff() { Message(new NMS_Command("machine:poweroff")); }

protected:
    void Cmd_SetIP(NetworkMessageStruct* msg);
    void Cmd_Command(NetworkMessageStruct* msg);
    
    void SendNetworkCommand(NMS_Command* msg, NetworkResult& result);
    void UpdateHostName();

    void Run();
    bool TryConnect();

    void GetResults(NetworkResult &result);

    std::string m_ipAddress;
    volatile bool m_terminate = false;

    bool m_connected = false;
    struct NET_Address* m_address = nullptr;
    std::string m_hostName;

    static const int MessageListSize = 16384;
    NetworkMessageStruct* m_messageList[MessageListSize];
    volatile int m_writeMsgIdx = 0;
    volatile int m_readMsgIdx = 0;
    std::counting_semaphore<256> m_signalMsg{ 0 };
};
