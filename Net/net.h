#pragma once

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#pragma comment(lib, "Ws2_32.lib")

#elif defined(__linux__)

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

using SOCKET = int;
using PCSTR = const char*;

#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)

#else
#error Unsupported platform
#endif

#include <thread>
#include <vector>
#include <mutex>
#include <queue>
#include <string>

#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "27016"
#define WORKER_THREAD_COUNT 4

#define MAX_MESSAGE_SIZE 1024 * 1024
#define PACKET_TYPE_SIZE sizeof(uint16_t)
#define PACKET_LENGTH_SIZE sizeof(uint32_t)
#define PACKET_HEADER_SIZE (PACKET_TYPE_SIZE + PACKET_LENGTH_SIZE)

class WorkerThread;
class Server;
class Client;

bool set_non_blocking(SOCKET _socket);

void server_worker(Server* server, WorkerThread* workerThread);

void client_worker(Client* client, WorkerThread* workerThread);
enum class PacketType : uint16_t {
    Null,
    Connect,
    Disconnect,
    Message,
    PlayerMove,
    RoomJoin,
    RoomLeave
};

struct Packet { 
    PacketType type = PacketType::Null;
    std::string data;
};

std::string encodePacket(const Packet& packet); // Type(2) + Size(4) + Data
bool decodePacket(std::vector<char>& receiveBuffer, Packet& packet);

struct SendMessageData {
    Packet packet;
    std::string data;
    size_t sentBytes = 0;
};

struct SocketData {
    SOCKET _socket;
    std::queue<SendMessageData> messageQueue;
    std::mutex queueMutex;
    std::vector<char> receiveBuffer;
};

struct NetEvent {
    enum Type {
        Null,
        Connected,
        Accepted,
        Received,
        Sent,
        Closed,
        Error
    };

    Type type;
    SOCKET _socket;
    Packet packet = {PacketType::Null, ""};

    NetEvent(
        Type eventType,
        SOCKET eventSocket,
        Packet eventpacket = {PacketType::Null, ""}
    );
};

class WorkerThread {
    std::thread workerThread;
    std::vector<SocketData*> sockets = std::vector<SocketData*>();
    std::mutex socketsMutex;
    bool running = false;

public:
    void start_server(Server* server);
    void start_client(Client* client);

    void addSocket(SOCKET _socket);
    void removeSocket(SOCKET _socket);

    void stop();

    bool isRunning() const;

    std::vector<SocketData*> getSockets();

    void enqueueMessage(
        SOCKET sourceSocket,
        PacketType type,
        const std::string& message
    );
};

class Server {
    bool running = false;

    std::vector<WorkerThread*> workerThreads;

    std::queue<NetEvent> eventQueue;
    std::mutex eventQueueMutex;

#ifdef _WIN32
    WSADATA wsaData;
#endif

    int iResult;

    PCSTR port;
    SOCKET ListenSocket = INVALID_SOCKET;

    int nextWorkerThreadIndex = 0;

public:
    Server(PCSTR port = DEFAULT_PORT);

    bool init();

    void update();

    void sendMessage(
        SOCKET _socket,
        PacketType type,
        const std::string& message
    );

    void pollEvents(NetEvent* event);

    void pushEvent(NetEvent event);

    ~Server();
};

class Client {
    bool running = false;
    bool connected = false;

    SOCKET clientSocket = INVALID_SOCKET;

    WorkerThread* workerThread = nullptr;

    std::queue<NetEvent> eventQueue;
    std::mutex eventQueueMutex;

#ifdef _WIN32
    WSADATA wsaData;
#endif

public:
    bool init();

    bool connectToServer(
        PCSTR address,
        PCSTR port = DEFAULT_PORT
    );

    void sendMessage(
        PacketType type,
        const std::string& message
    );

    bool pollEvent(NetEvent& event);

    void pushEvent(NetEvent event);

    bool isRunning() const;

    bool isConnected() const;

    void setConnected(bool value);

    ~Client();
};
