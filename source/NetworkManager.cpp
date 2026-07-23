#include "common.h"
#include "NetworkManager.h"
#include "LogManager.h"
#include "SDL3_net/SDL_net.h"
#include <map>
#include <vector>
#include "JSON.h"

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
    if (m_hostName.empty())
    {
        UpdateHostName();
    }

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
    status.m_hostName = m_hostName;
}

void NetworkManager::Cmd_SetIP(NetworkMessageStruct* msg)
{
    auto msgSetIP = (NMS_SetIP*)msg;
    if (msgSetIP->m_ip == m_ipAddress)
        return;

    m_connected = false;
    m_ipAddress = msgSetIP->m_ip;

    if (m_address)
    {
        NET_UnrefAddress(m_address);
        m_address = nullptr;
    }

    m_address = NET_ResolveHostname(m_ipAddress.c_str());
    if (!m_address)
    {
        Log(LogGroup::System, "Could not begin resolving {}: {}", m_ipAddress, SDL_GetError());
        return;
    }

    const NET_Status resolveStatus = NET_WaitUntilResolved(m_address, 2000);
    if (resolveStatus != NET_SUCCESS)
    {
        Log(LogGroup::System, "Could not resolve {}: {}", m_ipAddress, resolveStatus == NET_WAITING ? "Timed out" : SDL_GetError());
        NET_UnrefAddress(m_address);
        return;
    }

    UpdateHostName();
}

void NetworkManager::UpdateHostName()
{
    // query the host info
    NetworkResult result;
    auto cmd = new NMS_Command("info", nullptr, 0, "", true);
    SendNetworkCommand(cmd, result);

    if (result.mem && *result.mem)
    {
        JSON json;
        json.Parse(result.mem, (int)result.size);
        m_hostName = json.FindString("hostname");
        m_connected = true;
    }
}

void NetworkManager::SendNetworkCommand(NMS_Command* msg, NetworkResult &result)
{
    // ABORT if we don't have an address
    if (m_ipAddress.empty())
    {
        Log(LogGroup::System, "C64U IP Address not set -- cannot connect");
        m_connected = false;
        return;
    }

    // connect to C64U
    NET_StreamSocket *socket = NET_CreateClient(m_address, 80, 0);
    if (!socket)
    {
        Log(LogGroup::System, "Could not create socket for {}: {}", m_ipAddress, SDL_GetError());
        m_connected = false;
        return;
    }
    const NET_Status connectStatus = NET_WaitUntilConnected(socket, 2000);
    if (connectStatus != NET_SUCCESS)
    {
        Log(LogGroup::System, "Could not connect to C64U at {}: {}", m_ipAddress, connectStatus == NET_WAITING ? "Timed out" : SDL_GetError());
        NET_DestroyStreamSocket(socket);
        m_connected = false;
        return;
    }

    // send our command (PUSH/PUT/GET)

    std::string pushtype = msg->m_isGet ? "GET" : (msg->m_contentSize > 0 ? "POST" : "PUT");

    std::string request;
    if (msg->m_filename.empty())
    {
        request = std::format(
            "{} /v1/{} HTTP/1.1\r\n"
            "Host: {}\r\n"
            "Content-Length: {}\r\n"
            "Connection: close\r\n"
            "\r\n", pushtype, msg->m_command, m_ipAddress, msg->m_contentSize);
    }
    else
    {
        request = std::format(
            "{} /v1/{} HTTP/1.1\r\n"
            "Host: {}\r\n"
            "Content-Length: {}\r\n"
            "Content-Type: application/octet-stream\r\n"
            "Content-Disposition: attachment; filename=\"{}\"\r\n"
            "Connection: close\r\n"
            "\r\n", pushtype, msg->m_command, m_ipAddress, msg->m_contentSize, msg->m_filename);
    }

    if (!NET_WriteToStreamSocket(socket, request.data(), static_cast<int>(request.size())))
    {
        Log(LogGroup::System, "Could not send cmd request: {}", SDL_GetError());
        NET_DestroyStreamSocket(socket);
        return;
    }

    // send our content if we have any

    if (msg->m_contentSize > 0)
    {
        if (!NET_WriteToStreamSocket(socket, msg->m_contentMem, static_cast<int>(msg->m_contentSize)))
        {
            Log(LogGroup::System, "Could not send content request: {}", SDL_GetError());
            NET_DestroyStreamSocket(socket);
            return;
        }
    }

    // get results
    char buffer[4096];
    void* sockets[] = { socket };
    const int ready = NET_WaitUntilInputAvailable(sockets, 1, 200);
    const int bytesRead = NET_ReadFromStreamSocket(socket, buffer, sizeof(buffer));

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

    // get content from c64u
    result.amountRecieved = 0;
    if (result.size > 0)
    {
        result.mem = new u8[result.size];
        memset(result.mem, 0, result.size);
        while (result.amountRecieved < result.size)
        {
            NET_WaitUntilInputAvailable(sockets, 1, 200);
            int contentSizeRead = NET_ReadFromStreamSocket(socket, result.mem + result.amountRecieved, (int)(result.size - result.amountRecieved));
            if (contentSizeRead == 0)
                break;
            result.amountRecieved += contentSizeRead;
            if (contentSizeRead != result.size)
            {
                Log(LogGroup::System, "Read Results [{}] size: {} / {}", contentSizeRead, result.amountRecieved, result.size);
            }
        }
    }

    // finished with this connection
    NET_DestroyStreamSocket(socket);
}

void NetworkManager::Cmd_Command(NetworkMessageStruct* msg)
{
    auto msgCmd = (NMS_Command*)msg;
    Log(LogGroup::System, "Cmd: {} Content: {}", msgCmd->m_command, msgCmd->m_contentSize);

    NetworkResult result;
    SendNetworkCommand(msgCmd, result);
}



