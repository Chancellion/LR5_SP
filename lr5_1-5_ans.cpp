//   1) [Task 1] TCP server (single-thread):
//      win_net_lab.exe tcp_server <port>
//   2) [Task 2] TCP client:
//      win_net_lab.exe tcp_client <server_ip> <port> <message>
//   3) [Task 3] TCP server (multi-thread):
//      win_net_lab.exe tcp_server_mt <port>
//   4) [Task 4] UDP chat:
//      Server: win_net_lab.exe udp_server <port>
//      Client: win_net_lab.exe udp_client <server_ip> <port> <nickname>
//      (type messages; 'exit' to quit)
//   5) [Task 5] TCP JSON:
//      Server: win_net_lab.exe tcp_json_server <port>
//      Client: win_net_lab.exe tcp_json_client <server_ip> <port> <name> <age> <city>


#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <process.h>   // _beginthreadex
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstdio>
#include <string>
#include <vector>
#include <iostream>
#include <sstream>

#pragma comment(lib, "Ws2_32.lib")

// Optional JSON (task 5). If not available, define a tiny stub.
#ifdef USE_JSONCPP
#include <json/json.h>
#endif

// UTF-8 helpers
static std::string WideToUtf8(const std::wstring& w) {
    if (w.empty()) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), s.data(), len, nullptr, nullptr);
    return s;
}
static std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), w.data(), len);
    return w;
}

static void WSAInit() {
    WSADATA wsa{};
    int r = WSAStartup(MAKEWORD(2,2), &wsa);
    if (r != 0) {
        std::wcerr << L"WSAStartup failed: " << r << L"\n";
        exit(1);
    }
}
static void WSACleanupSafe() { WSACleanup(); }

// Common TCP create/bind/listen
static SOCKET CreateListenSocket(const wchar_t* port) {
    addrinfoW hints{};
    hints.ai_family = AF_INET;          // IPv4
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    addrinfoW* res = nullptr;
    int r = GetAddrInfoW(nullptr, port, &hints, &res);
    if (r != 0) {
        std::wcerr << L"GetAddrInfoW failed: " << r << L"\n";
        return INVALID_SOCKET;
    }
    SOCKET s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (s == INVALID_SOCKET) {
        std::wcerr << L"socket failed: " << WSAGetLastError() << L"\n";
        FreeAddrInfoW(res);
        return INVALID_SOCKET;
    }

    // Reuse address
    BOOL opt = TRUE;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    if (bind(s, res->ai_addr, (int)res->ai_addrlen) == SOCKET_ERROR) {
        std::wcerr << L"bind failed: " << WSAGetLastError() << L"\n";
        FreeAddrInfoW(res);
        closesocket(s);
        return INVALID_SOCKET;
    }
    FreeAddrInfoW(res);

    if (listen(s, SOMAXCONN) == SOCKET_ERROR) {
        std::wcerr << L"listen failed: " << WSAGetLastError() << L"\n";
        closesocket(s);
        return INVALID_SOCKET;
    }
    return s;
}

static SOCKET ConnectTcp(const wchar_t* ip, const wchar_t* port) {
    addrinfoW hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfoW* res = nullptr;
    int r = GetAddrInfoW(ip, port, &hints, &res);
    if (r != 0) {
        std::wcerr << L"GetAddrInfoW failed: " << r << L"\n";
        return INVALID_SOCKET;
    }
    SOCKET s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (s == INVALID_SOCKET) {
        std::wcerr << L"socket failed: " << WSAGetLastError() << L"\n";
        FreeAddrInfoW(res);
        return INVALID_SOCKET;
    }
    if (connect(s, res->ai_addr, (int)res->ai_addrlen) == SOCKET_ERROR) {
        std::wcerr << L"connect failed: " << WSAGetLastError() << L"\n";
        closesocket(s); FreeAddrInfoW(res);
        return INVALID_SOCKET;
    }
    FreeAddrInfoW(res);
    return s;
}

static bool SendAll(SOCKET s, const char* data, int len) {
    int sent = 0;
    while (sent < len) {
        int r = send(s, data + sent, len - sent, 0);
        if (r == SOCKET_ERROR) return false;
        sent += r;
    }
    return true;
}

// ----- [Task 1] TCP server (single-thread) -----
static int Task1_TcpServer(const wchar_t* port) {
    std::wcout << L"[Task 1] TCP server on port " << port << L"\n";
    WSAInit();
    SOCKET ls = CreateListenSocket(port);
    if (ls == INVALID_SOCKET) { WSACleanupSafe(); return 1; }

    std::wcout << L"Waiting for a client...\n";
    SOCKET cs = accept(ls, nullptr, nullptr);
    if (cs == INVALID_SOCKET) {
        std::wcerr << L"accept failed: " << WSAGetLastError() << L"\n";
        closesocket(ls); WSACleanupSafe(); return 1;
    }
    char buf[1024];
    int r = recv(cs, buf, sizeof(buf), 0);
    if (r > 0) {
        std::string msg(buf, buf + r);
        std::wcout << L"Received: " << Utf8ToWide(msg) << L"\n";
        std::string reply = "Echo: " + msg;
        SendAll(cs, reply.c_str(), (int)reply.size());
    } else {
        std::wcerr << L"recv failed or closed: " << WSAGetLastError() << L"\n";
    }

    closesocket(cs);
    closesocket(ls);
    WSACleanupSafe();
    return 0;
}

// ----- [Task 2] TCP client -----
static int Task2_TcpClient(const wchar_t* ip, const wchar_t* port, const wchar_t* msgW) {
    std::wcout << L"[Task 2] TCP client to " << ip << L":" << port << L"\n";
    WSAInit();
    SOCKET s = ConnectTcp(ip, port);
    if (s == INVALID_SOCKET) { WSACleanupSafe(); return 1; }

    std::string msg = WideToUtf8(msgW);
    if (!SendAll(s, msg.c_str(), (int)msg.size())) {
        std::wcerr << L"send failed: " << WSAGetLastError() << L"\n";
        closesocket(s); WSACleanupSafe(); return 1;
    }
    char buf[1024];
    int r = recv(s, buf, sizeof(buf), 0);
    if (r > 0) {
        std::string reply(buf, buf + r);
        std::wcout << L"Reply: " << Utf8ToWide(reply) << L"\n";
    } else {
        std::wcerr << L"recv failed or closed: " << WSAGetLastError() << L"\n";
    }

    closesocket(s);
    WSACleanupSafe();
    return 0;
}

// ----- [Task 3] TCP server (multi-thread) -----
struct ClientCtx {
    SOCKET s;
    HANDLE hMutex; // synchronize console output or shared resources
};

static unsigned __stdcall ClientThread(void* p) {
    ClientCtx* ctx = (ClientCtx*)p;
    char buf[2048];
    for (;;) {
        int r = recv(ctx->s, buf, sizeof(buf), 0);
        if (r <= 0) break;
        std::string msg(buf, buf + r);

        WaitForSingleObject(ctx->hMutex, INFINITE);
        std::wcout << L"[Client] " << Utf8ToWide(msg) << L"\n";
        ReleaseMutex(ctx->hMutex);

        std::string reply = "Echo MT: " + msg;
        if (!SendAll(ctx->s, reply.c_str(), (int)reply.size())) break;
    }
    closesocket(ctx->s);
    delete ctx;
    return 0;
}

static int Task3_TcpServerMT(const wchar_t* port) {
    std::wcout << L"[Task 3] TCP server (multi-thread) on port " << port << L"\n";
    WSAInit();
    SOCKET ls = CreateListenSocket(port);
    if (ls == INVALID_SOCKET) { WSACleanupSafe(); return 1; }

    HANDLE hMutex = CreateMutexW(nullptr, FALSE, nullptr);
    if (!hMutex) { std::wcerr << L"CreateMutex failed\n"; closesocket(ls); WSACleanupSafe(); return 1; }

    std::wcout << L"Accepting multiple clients... (Ctrl+C to stop)\n";
    for (;;) {
        SOCKET cs = accept(ls, nullptr, nullptr);
        if (cs == INVALID_SOCKET) {
            std::wcerr << L"accept failed: " << WSAGetLastError() << L"\n";
            break;
        }
        auto* ctx = new ClientCtx{ cs, hMutex };
        uintptr_t th = _beginthreadex(nullptr, 0, &ClientThread, ctx, 0, nullptr);
        if (!th) {
            std::wcerr << L"_beginthreadex failed\n";
            closesocket(cs); delete ctx; break;
        }
        CloseHandle((HANDLE)th);
    }
    CloseHandle(hMutex);
    closesocket(ls);
    WSACleanupSafe();
    return 0;
}

// ----- [Task 4] UDP chat (server/client) -----
static SOCKET CreateUdpServer(const wchar_t* port) {
    addrinfoW hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    hints.ai_flags = AI_PASSIVE;
    addrinfoW* res = nullptr;
    if (GetAddrInfoW(nullptr, port, &hints, &res) != 0) return INVALID_SOCKET;
    SOCKET s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (s == INVALID_SOCKET) { FreeAddrInfoW(res); return INVALID_SOCKET; }
    if (bind(s, res->ai_addr, (int)res->ai_addrlen) == SOCKET_ERROR) {
        closesocket(s); FreeAddrInfoW(res); return INVALID_SOCKET;
    }
    FreeAddrInfoW(res);
    return s;
}
static SOCKET CreateUdpClient(const wchar_t* ip, const wchar_t* port, sockaddr_in& dest) {
    addrinfoW hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    addrinfoW* res = nullptr;
    if (GetAddrInfoW(ip, port, &hints, &res) != 0) return INVALID_SOCKET;
    SOCKET s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (s == INVALID_SOCKET) { FreeAddrInfoW(res); return INVALID_SOCKET; }
    dest = *(sockaddr_in*)res->ai_addr;
    FreeAddrInfoW(res);
    return s;
}

static int Task4_UdpServer(const wchar_t* port) {
    std::wcout << L"[Task 4] UDP chat server on port " << port << L"\n";
    WSAInit();
    SOCKET s = CreateUdpServer(port);
    if (s == INVALID_SOCKET) { std::wcerr << L"UDP bind failed\n"; WSACleanupSafe(); return 1; }

    std::wcout << L"Listening UDP messages... (Ctrl+C to stop)\n";
    char buf[2048];
    for (;;) {
        sockaddr_in from{}; int fromlen = sizeof(from);
        int r = recvfrom(s, buf, sizeof(buf), 0, (sockaddr*)&from, &fromlen);
        if (r == SOCKET_ERROR) { std::wcerr << L"recvfrom error: " << WSAGetLastError() << L"\n"; break; }
        std::string msg(buf, buf + r);
        std::wcout << L"[UDP] " << Utf8ToWide(msg) << L"\n";
        // Echo back
        sendto(s, msg.c_str(), (int)msg.size(), 0, (sockaddr*)&from, fromlen);
    }
    closesocket(s);
    WSACleanupSafe();
    return 0;
}

static int Task4_UdpClient(const wchar_t* ip, const wchar_t* port, const wchar_t* nicknameW) {
    std::wcout << L"[Task 4] UDP chat client to " << ip << L":" << port << L"\n";
    WSAInit();
    sockaddr_in dest{};
    SOCKET s = CreateUdpClient(ip, port, dest);
    if (s == INVALID_SOCKET) { std::wcerr << L"UDP socket failed\n"; WSACleanupSafe(); return 1; }

    std::wcout << L"Type messages, 'exit' to quit.\n";
    std::wstring line;
    while (true) {
        std::getline(std::wcin, line);
        if (line == L"exit") break;
        std::string payload = WideToUtf8(std::wstring(nicknameW) + L": " + line);
        int r = sendto(s, payload.c_str(), (int)payload.size(), 0, (sockaddr*)&dest, sizeof(dest));
        if (r == SOCKET_ERROR) { std::wcerr << L"sendto error: " << WSAGetLastError() << L"\n"; break; }
        char buf[2048];
        sockaddr_in from{}; int fromlen = sizeof(from);
        r = recvfrom(s, buf, sizeof(buf), 0, (sockaddr*)&from, &fromlen);
        if (r > 0) {
            std::string reply(buf, buf + r);
            std::wcout << L"[Echo] " << Utf8ToWide(reply) << L"\n";
        }
    }
    closesocket(s);
    WSACleanupSafe();
    return 0;
}

// ----- [Task 5] TCP JSON serialization -----
static int Task5_TcpJsonServer(const wchar_t* port) {
    std::wcout << L"[Task 5] TCP JSON server on port " << port << L"\n";
    WSAInit();
    SOCKET ls = CreateListenSocket(port);
    if (ls == INVALID_SOCKET) { WSACleanupSafe(); return 1; }

    std::wcout << L"Waiting JSON client...\n";
    SOCKET cs = accept(ls, nullptr, nullptr);
    if (cs == INVALID_SOCKET) { std::wcerr << L"accept failed\n"; closesocket(ls); WSACleanupSafe(); return 1; }

    // Read a JSON blob (simple framing: length prefix uint32, network order)
    uint32_t lenNet = 0;
    int r = recv(cs, (char*)&lenNet, sizeof(lenNet), 0);
    if (r != sizeof(lenNet)) { std::wcerr << L"recv length failed\n"; goto done; }
    uint32_t len = ntohl(lenNet);
    if (len > 10 * 1024 * 1024) { std::wcerr << L"payload too large\n"; goto done; }
    std::vector<char> buf(len);
    int got = 0;
    while (got < (int)len) {
        int rr = recv(cs, buf.data() + got, len - got, 0);
        if (rr <= 0) { std::wcerr << L"recv body failed\n"; goto done; }
        got += rr;
    }
    std::string jsonStr(buf.begin(), buf.end());
    std::wcout << L"Received JSON: " << Utf8ToWide(jsonStr) << L"\n";

    // Deserialize and display
#ifdef USE_JSONCPP
    Json::Value root;
    Json::CharReaderBuilder b;
    std::string errs;
    std::istringstream iss(jsonStr);
    if (!Json::parseFromStream(b, iss, &root, &errs)) {
        std::wcerr << L"JSON parse error: " << Utf8ToWide(errs) << L"\n";
        goto done;
    }
    std::wcout << L"Parsed fields:\n";
    std::wcout << L"  name: " << Utf8ToWide(root.get("name", "").asString()) << L"\n";
    std::wcout << L"  age: "  << root.get("age", 0).asInt() << L"\n";
    std::wcout << L"  city: " << Utf8ToWide(root.get("city", "").asString()) << L"\n";
    // Echo back augmented JSON
    root["status"] = "ok";
    Json::StreamWriterBuilder wb;
    std::string reply = Json::writeString(wb, root);
#else
    // Fallback: return a simple string acknowledging receipt
    std::string reply = std::string("{\"status\":\"ok\",\"received\":") + "\"" + jsonStr + "\"" + "}";
#endif
    uint32_t rlen = (uint32_t)reply.size();
    uint32_t rlenNet = htonl(rlen);
    SendAll(cs, (const char*)&rlenNet, sizeof(rlenNet));
    SendAll(cs, reply.c_str(), (int)reply.size());

done:
    closesocket(cs);
    closesocket(ls);
    WSACleanupSafe();
    return 0;
}

static int Task5_TcpJsonClient(const wchar_t* ip, const wchar_t* port,
                               const wchar_t* nameW, const wchar_t* ageW, const wchar_t* cityW) {
    std::wcout << L"[Task 5] TCP JSON client to " << ip << L":" << port << L"\n";
    WSAInit();
    SOCKET s = ConnectTcp(ip, port);
    if (s == INVALID_SOCKET) { WSACleanupSafe(); return 1; }

    // Build JSON
    std::string name = WideToUtf8(nameW);
    int age = _wtoi(ageW);
    std::string city = WideToUtf8(cityW);

#ifdef USE_JSONCPP
    Json::Value root;
    root["name"] = name;
    root["age"]  = age;
    root["city"] = city;
    Json::StreamWriterBuilder wb;
    std::string payload = Json::writeString(wb, root);
#else
    std::ostringstream oss;
    oss << "{\"name\":\"" << name << "\",\"age\":" << age << ",\"city\":\"" << city << "\"}";
    std::string payload = oss.str();
#endif

    // Send length-prefixed JSON
    uint32_t len = (uint32_t)payload.size();
    uint32_t lenNet = htonl(len);
    if (!SendAll(s, (const char*)&lenNet, sizeof(lenNet)) ||
        !SendAll(s, payload.c_str(), (int)payload.size())) {
        std::wcerr << L"send failed\n"; closesocket(s); WSACleanupSafe(); return 1;
    }

    // Receive reply
    uint32_t rlenNet = 0;
    int r = recv(s, (char*)&rlenNet, sizeof(rlenNet), 0);
    if (r != sizeof(rlenNet)) { std::wcerr << L"recv length failed\n"; closesocket(s); WSACleanupSafe(); return 1; }
    uint32_t rlen = ntohl(rlenNet);
    std::vector<char> buf(rlen);
    int got = 0;
    while (got < (int)rlen) {
        int rr = recv(s, buf.data() + got, rlen - got, 0);
        if (rr <= 0) { std::wcerr << L"recv body failed\n"; closesocket(s); WSACleanupSafe(); return 1; }
        got += rr;
    }
    std::string reply(buf.begin(), buf.end());
    std::wcout << L"Server reply JSON: " << Utf8ToWide(reply) << L"\n";

    closesocket(s);
    WSACleanupSafe();
    return 0;
}

// ---- Entry point ----
static void PrintUsage() {
    std::wcout << L"Usage:\n"
               << L"  [Task 1] tcp_server <port>\n"
               << L"  [Task 2] tcp_client <ip> <port> <message>\n"
               << L"  [Task 3] tcp_server_mt <port>\n"
               << L"  [Task 4] udp_server <port>\n"
               << L"           udp_client <ip> <port> <nickname>\n"
               << L"  [Task 5] tcp_json_server <port>\n"
               << L"           tcp_json_client <ip> <port> <name> <age> <city>\n";
}

int wmain(int argc, wchar_t* argv[]) {
    if (argc < 2) { PrintUsage(); return 0; }
    std::wstring cmd = argv[1];

    if (cmd == L"tcp_server" && argc >= 3) {
        return Task1_TcpServer(argv[2]); // [Task 1]
    } else if (cmd == L"tcp_client" && argc >= 5) {
        return Task2_TcpClient(argv[2], argv[3], argv[4]); // [Task 2]
    } else if (cmd == L"tcp_server_mt" && argc >= 3) {
        return Task3_TcpServerMT(argv[2]); // [Task 3]
    } else if (cmd == L"udp_server" && argc >= 3) {
        return Task4_UdpServer(argv[2]); // [Task 4]
    } else if (cmd == L"udp_client" && argc >= 5) {
        return Task4_UdpClient(argv[2], argv[3], argv[4]); // [Task 4]
    } else if (cmd == L"tcp_json_server" && argc >= 3) {
        return Task5_TcpJsonServer(argv[2]); // [Task 5]
    } else if (cmd == L"tcp_json_client" && argc >= 7) {
        return Task5_TcpJsonClient(argv[2], argv[3], argv[4], argv[5], argv[6]); // [Task 5]
    } else {
        PrintUsage();
        return 1;
    }
}
