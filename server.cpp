#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <WinSock2.h>
#include <WS2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <mutex>
#include <string>
#include <vector>
#include <thread>
#include <algorithm>
#include <chrono>
#include <unordered_map>
#include <atomic>

using namespace std;
namespace fs = std::filesystem;
using steady_clock = std::chrono::steady_clock;

const string SERVER_IP = "0.0.0.0";
const int PORT = 5050;
const int MAX_CLIENTS = 4;
const int TIMEOUT_SEC = 60;
const string LOG_FILE = "server_stats.txt";
const string FILE_DIR = "server_files";
const int LOG_INTERVAL_SEC = 5;
const int BUFFER = 4096;

struct ClientInfo
{
    SOCKET sock;
    string ip;
    int port;
    bool isAdmin;
    int message = 0;
    uint64_t bytes = 0;
    steady_clock::time_point lastActive;
};

vector<ClientInfo> clients;
mutex clients_mtx;
atomic<bool> running(true);

bool sendAll(SOCKET s, const char *data, int len)
{
    int sent = 0;
    while (sent < len)
    {
        int n = send(s, data + sent, len - sent, 0);
        if (n == SOCKET_ERROR || n == 0)
        {
            return false;
        }
        sent += n;
    }
    return true;
}

void removeClient(SOCKET s)
{
    lock_guard<mutex> lock(clients_mtx);
    clients.erase(remove_if(clients.begin(), clients.end(),
                            [&](const ClientInfo &c)
                            { return c.sock == s; }),
                  clients.end());
    closesocket(s);
}

int main()
{
    if (!fs::exists(FILE_DIR))
        fs::create_directories(FILE_DIR);

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        cerr << "WSAStartup failed\n";
        return 1;
    }

    SOCKET listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSock == INVALID_SOCKET)
    {
        cerr << "Socket failed\n";
        WSACleanup();
        return 1;
    }

    sockaddr_in service{};
    service.sin_family = AF_INET;
    service.sin_addr.s_addr = INADDR_ANY;
    service.sin_port = htons(PORT);

    if (::bind(listenSock, (SOCKADDR *)&service, sizeof(service)) == SOCKET_ERROR)
    {
        cerr << "Bind failed\n";
        closesocket(listenSock);
        WSACleanup();
        return 1;
    }

    if (listen(listenSock, SOMAXCONN) == SOCKET_ERROR)
    {
        cerr << "Listen failed\n";
        closesocket(listenSock);
        WSACleanup();
        return 1;
    }

    cout << "Server listening on port " << PORT << "\n";

    while (running)
    {
        sockaddr_in clientInfo;
        int addrsize = sizeof(clientInfo);
        SOCKET clientSock = accept(listenSock, (SOCKADDR *)&clientInfo, &addrsize);
        if (clientSock == INVALID_SOCKET)
        {
            continue;
        }
        string cliIp = inet_ntoa(clientInfo.sin_addr);
        int cliPort = ntohs(clientInfo.sin_port);

        {
            lock_guard<mutex> lock(clients_mtx);
            if ((int)clients.size() >= MAX_CLIENTS)
            {
                string msg = "Server full. Connection refused.\n";
                sendAll(clientSock, msg.c_str(), (int)msg.size());
                closesocket(clientSock);
                continue;
            }
        }

        cout << "New connection from " << cliIp << ":" << cliPort << "\n";
    }
    closesocket(listenSock);
    WSACleanup();
    return 0;
}