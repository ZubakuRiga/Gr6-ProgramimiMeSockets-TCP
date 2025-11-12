#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <string>
#include <thread>
#include <atomic>
#include <chrono>

using namespace std;
namespace fs = std::filesystem;
const int BUFFER = 4096;
atomic<bool> running(true);

// send all helper
bool sendAll(SOCKET s, const char *data, int len)
{
    int sent = 0;
    while (sent < len)
    {
        int n = send(s, data + sent, len - sent, 0);
        if (n <= 0)
            return false;
        sent += n;
    }
    return true;
}

int main()
{
    string serverIp;
    int port;

    cout << "Server IP: ";
    getline(cin, serverIp);
    cout << "Server PORT: ";
    string portStr;
    getline(cin, portStr);
    port = stoi(portStr);

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        cerr << "WSAStartup failed\n";
        return 1;
    }

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET)
    {
        cerr << "Socket creation failed\n";
        WSACleanup();
        return 1;
    }

    sockaddr_in serv{};
    serv.sin_family = AF_INET;
    inet_pton(AF_INET, serverIp.c_str(), &serv.sin_addr);
    serv.sin_port = htons(port);

    if (connect(sock, (sockaddr *)&serv, sizeof(serv)) == SOCKET_ERROR)
    {
        cerr << "Connection failed\n";
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    cout << "Connected to server successfully!\n";

    running = false;
    closesocket(sock);
    WSACleanup();
    return 0;
}
