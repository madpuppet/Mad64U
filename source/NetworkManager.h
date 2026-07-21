#pragma once

#include "Singleton.h"
#include <semaphore>

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
    NMS_Command(const std::string& command, u8 *mem=nullptr, size_t size=0) : NetworkMessageStruct(NetworkMessage::Command), m_command(command), m_contentMem(mem), m_contentSize(size) {}

    std::string m_command;
    u8* m_contentMem = nullptr;
    size_t m_contentSize = 0;
};

#define NETWORK_MESSAGE_LIST_SIZE 256

struct NetworkStatus
{
    bool m_connected;
    std::string m_ipAddress;
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

protected:
    void Cmd_SetIP(NetworkMessageStruct* msg);
    void Cmd_Command(NetworkMessageStruct* msg);

    void Run();
    bool TryConnect();

    void GetResults(NetworkResult &result);

    std::string m_ipAddress;
    volatile bool m_terminate = false;

    bool m_connected = false;
    struct NET_StreamSocket* m_socket = nullptr;
    struct NET_Address* m_address = nullptr;


    static const int MessageListSize = 16384;
    NetworkMessageStruct* m_messageList[MessageListSize];
    volatile int m_writeMsgIdx = 0;
    volatile int m_readMsgIdx = 0;
    std::counting_semaphore<256> m_signalMsg{ 0 };
};
