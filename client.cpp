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
void recvLoop(SOCKET s, bool isAdmin)
{
    char buf[BUFFER];
    while (running)
    {
        int n = recv(s, buf, BUFFER, 0);
        if (n <= 0)
        {
            cout << "\n[!] Server disconnected.\n";
            running = false;
            return;
        }
        string first(buf, buf + min(n, 9));
        if (first == "FILESIZE ")
        {
            string header(buf, buf + n);
            size_t pos = header.find('\n');
            while (pos == string::npos)
            {
                int m = recv(s, buf, BUFFER, 0);
                if (m <= 0)
                {
                    running = false;
                    return;
                }
                header.append(buf, buf + m);
                pos = header.find('\n');
            }
            string line = header.substr(0, pos);
            istringstream iss(line);
            string tag;
            uint64_t fsize = 0;
            iss >> tag >> fsize;
            cout << "\n[Server] Incoming file of " << fsize << " bytes. Save as: ";
            string outname;
            getline(cin, outname);
            if (outname.empty())
                outname = "downloaded_file";
            ofstream out(outname, ios::binary);
            uint64_t remaining = fsize;
            string tail = header.substr(pos + 1);
            if (!tail.empty())
            {
                out.write(tail.data(), (streamsize)tail.size());
                remaining -= tail.size();
            }
            while (remaining > 0)
            {
                int got = recv(s, buf, (int)min<uint64_t>(BUFFER, remaining), 0);
                if (got <= 0)
                {
                    cout << "Download interrupted\n";
                    out.close();
                    running = false;
                    return;
                }
                out.write(buf, got);
                remaining -= got;
            }
            out.close();
            cout << "\n[Server] File saved as " << outname << "\n> ";
            cout.flush();
            continue;
        }

        cout << "\n[Server] ";
        cout.write(buf, n);
        cout << "\n> ";
        cout.flush();
    }
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

    cout << "Are you admin? (1=yes / 0=no): ";
    string adm;
    getline(cin, adm);
    bool isAdmin = (adm == "1");

    string role = "ROLE " + string(isAdmin ? "admin\n" : "user\n");
    sendAll(sock, role.c_str(), (int)role.size());

    thread(recvLoop, sock, isAdmin).detach();

    cout << "Komandat e disponueshme:\n";
    cout << "/list\n/read <filename>\n/upload <filename>\n/download <filename>\n/delete <filename>\n/search <keyword>\n/info <filename>\nSTATS (admin)\n";
    cout << "Shkruaj 'exit' për të dalë.\n";

    while (running)
    {
        cout << "> ";
        string line;
        getline(cin, line);

        if (!running)
            break;
        if (line.empty())
            continue;
        if (line == "exit")
            break;

        if (isAdmin && line.rfind("/upload ", 0) == 0)
        {
            istringstream iss(line);
            string cmd, fname;
            iss >> cmd >> fname;
            if (fname.empty())
            {
                cout << "Usage: /upload <filename>\n";
                continue;
            }
            if (!fs::exists(fname))
            {
                cout << "Local file not found\n";
                continue;
            }
            uint64_t fsize = fs::file_size(fname);
            string header = "/upload " + fs::path(fname).filename().string() + " " + to_string(fsize) + "\n";
            if (!sendAll(sock, header.c_str(), (int)header.size()))
                break;
            ifstream f(fname, ios::binary);
            char buf[BUFFER];
            while (f)
            {
                f.read(buf, BUFFER);
                streamsize r = f.gcount();
                if (r > 0 && !sendAll(sock, buf, (int)r))
                {
                    cerr << "Send failed\n";
                    break;
                }
            }
            cout << "Upload sent.\n";
            continue;
        }

        if (line.rfind("/download ", 0) == 0)
        {
            string out = line + "\n";
            if (!sendAll(sock, out.c_str(), (int)out.size()))
                break;
            cout << "[Request sent for download]\n";
            continue;
        }

        if (line == "STATS" && isAdmin)
        {
            if (!sendAll(sock, (line + "\n").c_str(), (int)line.size() + 1))
                break;
            continue;
        }

        if (line.rfind("/", 0) == 0 || (!isAdmin && line.rfind("/", 0) == 0))
        {
            string out = line + "\n";
            if (!sendAll(sock, out.c_str(), (int)out.size()))
                break;
            continue;
        }

        string msg = line + "\n";
        sendAll(sock, msg.c_str(), (int)msg.size());
    }

    running = false;
    closesocket(sock);
    WSACleanup();
    return 0;
}
