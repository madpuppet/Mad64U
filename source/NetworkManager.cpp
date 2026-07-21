#include "common.h"
#include "NetworkManager.h"
#include "LogManager.h"
#include "SDL3_net/SDL_net.h"

NetworkManager::NetworkManager()
{
    if (!NET_Init())
    {
        Log(LogGroup::Build, "NET_Init failed: {}", SDL_GetError());
        return;
    }

    std::thread([this]() { Run(); }).detach();
}

NetworkManager::~NetworkManager()
{
    NET_Quit();
}

void NetworkManager::Message(NetworkMessageStruct* msg)
{
    m_messageList[m_writeMsgIdx] = msg;
    m_writeMsgIdx = (m_writeMsgIdx + 1) % MessageListSize;
    m_signalMsg.release();
}

void NetworkManager::Run()
{
    while (true)
    {
        m_signalMsg.acquire();
        if (m_terminate)
            return;

        auto msg = m_messageList[m_readMsgIdx];
        m_readMsgIdx = (m_readMsgIdx + 1) % MessageListSize;
        switch (msg->m_msgType)
        {
            case NetworkMessage::SetIP:
                Cmd_SetIP(msg);
                break;
            case NetworkMessage::Command:
                Cmd_Command(msg);
                break;
        }
        delete msg;
    }
}

void NetworkManager::GetNetworkStatus(NetworkStatus& status)
{
    status.m_ipAddress = m_ipAddress;
    status.m_connected = m_connected;
}

void NetworkManager::Cmd_SetIP(NetworkMessageStruct* msg)
{
    auto msgSetIP = (NMS_SetIP*)msg;
    if (msgSetIP->m_ip == m_ipAddress)
    {
        // no change, if we are already connected just exit
        if (m_socket && NET_GetConnectionStatus(m_socket) == NET_SUCCESS)
            return;
    }

    m_ipAddress = msgSetIP->m_ip;

    if (m_address)
    {
        NET_UnrefAddress(m_address);
        m_address = nullptr;
    }

    if (m_socket)
    {
        NET_DestroyStreamSocket(m_socket);
        m_socket = nullptr;
    }

    TryConnect();
}

bool NetworkManager::TryConnect()
{
    // destroy the socket if we've lost connection
    if (m_socket)
    {
        if (NET_GetConnectionStatus(m_socket) == NET_SUCCESS)
        {
            Log(LogGroup::System, "Connection Status Success");
            return true;
        }

        Log(LogGroup::System, "Connection Status Failed - Destroying Socket");

        NET_DestroyStreamSocket(m_socket);
        m_socket = nullptr;
        m_connected = false;
    }

    // check we have an address
    if (m_ipAddress.empty())
    {
        Log(LogGroup::System, "C64U IP Address not set -- cannot connect");
        return false;
    }

    // check we have a resolved address
    if (!m_address)
    {
        m_address = NET_ResolveHostname(m_ipAddress.c_str());
        if (!m_address)
        {
            Log(LogGroup::System, "Could not begin resolving {}: {}", m_ipAddress, SDL_GetError());
            return false;
        }

        const NET_Status resolveStatus = NET_WaitUntilResolved(m_address, 2000);
        if (resolveStatus != NET_SUCCESS)
        {
            Log(LogGroup::System, "Could not resolve {}: {}", m_ipAddress, resolveStatus == NET_WAITING ? "Timed out" : SDL_GetError());
            NET_UnrefAddress(m_address);
            return false;
        }
    }

    m_socket = NET_CreateClient(m_address, 80, 0);
    if (!m_socket)
    {
        Log(LogGroup::System, "Could not create socket for {}: {}", m_ipAddress, SDL_GetError());
        return false;
    }
    const NET_Status connectStatus = NET_WaitUntilConnected(m_socket, 2000);
    if (connectStatus != NET_SUCCESS)
    {
        Log(LogGroup::System, "Could not connect to C64U at {}: {}", m_ipAddress, connectStatus == NET_WAITING ? "Timed out" : SDL_GetError());
        NET_DestroyStreamSocket(m_socket);
        m_socket = nullptr;
        return false;
    }
    Log(LogGroup::System, "Connected");

    m_connected = true;
    return true;
}

void NetworkManager::GetResults(NetworkResult &result)
{
    char buffer[4096];
    void* sockets[] = { m_socket };
    const int ready = NET_WaitUntilInputAvailable(sockets, 1, 200);
    const int bytesRead = NET_ReadFromStreamSocket(m_socket, buffer, sizeof(buffer));

    std::string line;
    if (bytesRead > 0)
    {
        buffer[bytesRead] = 0;
        for (int i = 0; i < bytesRead; i++)
        {
            if (buffer[i] == '\n')
            {
                if (line.starts_with("Connection:"))
                {
                    result.connection = line.substr(12);
                }
                else if (line.starts_with("Content-Length:"))
                {
                    result.size = std::stoi(line.substr(16));
                }
                else if (line.starts_with("Content-Type:"))
                {
                    result.contentType = line.substr(14);
                }
                line.clear();
            }
            else if (buffer[i] != '\t' && buffer[i] != '\r')
            {
                line.push_back(buffer[i]);
            }
        }
    }

    result.amountRecieved = 0;
    if (result.size > 0)
    {
        result.mem = new u8[result.size];
        memset(result.mem, 0, result.size);
        while (result.amountRecieved < result.size)
        {
            NET_WaitUntilInputAvailable(sockets, 1, 200);
            int contentSizeRead = NET_ReadFromStreamSocket(m_socket, result.mem + result.amountRecieved, (int)(result.size - result.amountRecieved));
            if (contentSizeRead == 0)
                break;
            result.amountRecieved += contentSizeRead;
            if (contentSizeRead != result.size)
            {
                Log(LogGroup::System, "Read Results [{}] size: {} / {}", contentSizeRead, result.amountRecieved, result.size);
            }
        }
    }

    if (result.connection == "close")
    {
        NET_DestroyStreamSocket(m_socket);
        m_socket = nullptr;
        m_connected = false;
    }

    Log(LogGroup::System, "Connection: {},  ContentType: {},  ContentSize: {}/{}", result.connection, result.contentType, result.amountRecieved, result.size);
}

void NetworkManager::Cmd_Command(NetworkMessageStruct* msg)
{
    auto msgCmd = (NMS_Command*)msg;

    if (!TryConnect())
        return;

    Log(LogGroup::System, "Send Command: {}  Content: {} bytes", msgCmd->m_command, msgCmd->m_contentSize);

    const std::string request = std::format(
        "PUT /v1/{} HTTP/1.1\r\n"
        "Host: {}\r\n"
        "Content-Length: 0\r\n"
        "Connection: close\r\n"
        "\r\n", msgCmd->m_command, m_ipAddress);

    if (!NET_WriteToStreamSocket(m_socket, request.data(), static_cast<int>(request.size())))
    {
        Log(LogGroup::System, "Could not send reset request: {}", SDL_GetError());
        NET_DestroyStreamSocket(m_socket);
        m_socket = nullptr;
        return;
    }

    NetworkResult result;
    GetResults(result);

    Log(LogGroup::System, "Result: connection {}, content {}, size {}", result.connection, result.contentType, result.size);
}



