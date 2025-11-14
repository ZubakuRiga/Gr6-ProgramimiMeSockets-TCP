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

void showHelp() {
    cout << "Komandat e disponueshme:\n";
    cout << "/list - Listo të gjitha skedarët\n";
    cout << "/read <filename> - Lexo një skedar\n";
    cout << "/upload <filename> - Ngarko një skedar (admin)\n";
    cout << "/download <filename> - Shkarko një skedar\n";
    cout << "/delete <filename> - Fshij një skedar (admin)\n";
    cout << "/search <keyword> - Kërko në skedarë\n";
    cout << "/info <filename> - Shfaq informacion për skedar\n";
    cout << "Shkruaj 'exit' për të dalë.\n";
}

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

// read a single line terminated by \n
bool recvLine(SOCKET s, string& out)
{
    out.clear();
    char ch;
    while (true)
    {
        int n = recv(s, &ch, 1, 0);
        if (n <= 0) return false;
        out.push_back(ch);
        if (ch == '\n') break;
    }
    return true;
}

void processCommand(ClientInfo& ci, const string& line) {
    SOCKET s = ci.sock;
    string cmd = line;
    
    // PERSONI 3: Pastro komandën
    while (!cmd.empty() && (cmd.back() == '\n' || cmd.back() == '\r')) 
        cmd.pop_back();
    if (cmd.empty()) return;

    // PERSONI 3: Komandat e adminit
    if (ci.isAdmin) {
        if (cmd == "STATS") {
            ostringstream oss;
            oss << "Active: " << clients.size() << "\n";
            for (auto& c : clients) {
                oss << c.ip << ":" << c.port << " admin=" << (c.isAdmin ? "y" : "n")
                    << " messages=" << c.messages << " bytes=" << c.bytes << "\n";
            }
            string out = oss.str();
            sendAll(s, out.c_str(), (int)out.size());
            return;
        }
        
        if (cmd.rfind("/list", 0) == 0) {
            string out;
            for (auto& p : fs::directory_iterator(FILE_DIR)) 
                out += p.path().filename().string() + "\n";
            sendAll(s, out.c_str(), (int)out.size());
            return;
        }
        
        if (cmd.rfind("/read ", 0) == 0) {
            string fname = cmd.substr(6);
            fs::path fp = fs::path(FILE_DIR) / fname;
            if (!fs::exists(fp)) { 
                sendAll(s, "ERROR: file not found\n", 21); 
                return; 
            }
            ifstream f(fp, ios::binary);
            ostringstream buf; 
            buf << f.rdbuf();
            string data = buf.str();
            sendAll(s, data.c_str(), (int)data.size());
            return;
        }
        
        if (cmd.rfind("/upload ", 0) == 0) {
            istringstream iss(cmd);
            string tag, fname; 
            size_t size = 0;
            iss >> tag >> fname >> size;
            
            if (fname.empty() || size == 0) { 
                sendAll(s, "ERROR: invalid upload header\n", 29); 
                return; 
            }
            
            fs::path fp = fs::path(FILE_DIR) / fname;
            ofstream out(fp, ios::binary);
            size_t remaining = size;
            const int BUF = 4096; 
            vector<char> buffer(BUF);
            
            while (remaining > 0) {
                int toRead = (int)min<size_t>(BUF, remaining);
                int n = recv(s, buffer.data(), toRead, 0);
                if (n <= 0) { 
                    removeClient(s); 
                    return; 
                }
                out.write(buffer.data(), n);
                remaining -= n;
                
                lock_guard<mutex> lock(clients_mtx);
                for (auto& c : clients) 
                    if (c.sock == s) c.bytes += n;
            }
            out.close();
            sendAll(s, "OK: upload complete\n", 20);
            return;
        }
    }

    // PERSONI 3: Jo-admin duke provuar komandë admini
    if (!ci.isAdmin && cmd.rfind("/", 0) == 0) {
        sendAll(s, "ERROR: Admin privileges required\n", 33);
        return;
    }

    // PERSONI 3: Transmetimi i mesazheve te të gjithë klientët
    string broadcastMsg = ci.ip + ":" + to_string(ci.port) + " says: " + cmd + "\n";
    lock_guard<mutex> lock(clients_mtx);
    for (auto& c : clients) {
        if (c.sock != s) {
            sendAll(c.sock, broadcastMsg.c_str(), (int)broadcastMsg.size());
        }
    }
}

void clientThread(SOCKET clientSock, string clientIp, int clientPort)
{
    ClientInfo ci{clientSock, clientIp, clientPort, false, 0, 0, steady_clock::now()};
    {
        lock_guard<mutex> lock(clients_mtx);
        clients.push_back(ci);
    }

    string header;
    if (!recvLine(clientSock, header))
    {
        removeClient(clientSock);
        return;
    }
    while (!header.empty() && (header.back() == '\n' || header.back() == '\r'))
        header.pop_back();
    bool adminFlag = (header == "ROLE admin");
    {
        lock_guard<mutex> lock(clients_mtx);
        for (auto &c : clients)
            if (c.sock == clientSock)
            {
                c.isAdmin = adminFlag;
                c.lastActive = steady_clock::now();
            }
    }
    string welcome = "Welcome. Admin=" + string(adminFlag ? "yes\n" : "no\n");
    sendAll(clientSock, welcome.c_str(), (int)welcome.size());

    while (true)
    {
        {
            lock_guard<mutex> lock(clients_mtx);
            auto it = find_if(clients.begin(), clients.end(), [&](const ClientInfo &c)
                              { return c.sock == clientSock; });
            if (it == clients.end())
                break;
            auto dur = chrono::duration_cast<chrono::seconds>(steady_clock::now() - it->lastActive).count();
            if (dur > TIMEOUT_SEC)
            {
                sendAll(clientSock, "Disconnected due to inactivity\n", 31);
                removeClient(clientSock);
                return;
            }
        }
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(clientSock, &readfds);
        timeval tv{0, 500000};
        int rv = select(0, &readfds, NULL, NULL, &tv);
        if (rv == SOCKET_ERROR)
        {
            removeClient(clientSock);
            return;
        }
        if (rv == 0)
            continue;
        string line;
        if (!recvLine(clientSock, line))
        {
            removeClient(clientSock);
            return;
        }
        {
            lock_guard<mutex> lock(clients_mtx);
            for (auto &c : clients)
            {
                if (c.sock == clientSock)
                {
                    c.messages++;
                    c.lastActive = steady_clock::now();
                    c.bytes += line.size();
                    ci = c;
                }
            }
        }
        processCommand(ci, line);
    }
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
        showHelp();
    }
    
    closesocket(listenSock);
    WSACleanup();
    return 0;
}