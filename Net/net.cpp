#include "net.h"

#include <stdlib.h>
#include <stdio.h>
#include <thread>
#include <vector>
#include <mutex>
#include <queue>
#include <chrono>
#include <cstring>

int get_socket_error() {
#ifdef _WIN32
    return WSAGetLastError();
#elif __linux__
    return errno;
#endif
}

bool is_would_block(int error) {
#ifdef _WIN32
    return error == WSAEWOULDBLOCK;
#elif __linux__
    return error == EWOULDBLOCK || error == EAGAIN;
#endif
}

void close_socket(SOCKET _socket) {
#ifdef _WIN32
    closesocket(_socket);
#elif __linux__
    close(_socket);
#endif
}

int get_send_flags() {
#ifdef _WIN32
    return 0;
#elif __linux__
    return MSG_NOSIGNAL;
#endif
}

int get_shutdown() {
#ifdef _WIN32
    return SD_BOTH;
#elif __linux__
    return SHUT_RDWR;
#endif
}

int get_select_nfds(SOCKET maxSocket) {
#ifdef _WIN32
    return 0;
#elif __linux__
    return maxSocket + 1;
#endif
}

std::string encodePacket(const Packet& packet) {
    if (packet.data.size() > MAX_MESSAGE_SIZE) {
        return "";
    }

    uint16_t type = htons((uint16_t)packet.type);

    uint32_t length = htonl(packet.data.size());

    std::string data;
    data.append((char*)(&type), sizeof(type));
    data.append((char*)(&length), sizeof(length));
    data.append(packet.data);

    return data; // Type(2) + Size(4) + Data
}

bool decodePacket(std::vector<char>& receiveBuffer, Packet& packet) {
    if (receiveBuffer.size() < PACKET_HEADER_SIZE) {
        return false;
    }

    uint16_t networkType = 0;
    uint32_t networkLength = 0;
    std::memcpy(&networkType, receiveBuffer.data(), sizeof(networkType));
    std::memcpy(&networkLength, receiveBuffer.data() + PACKET_TYPE_SIZE, sizeof(networkLength));

    packet.type = static_cast<PacketType>(ntohs(networkType));
    uint32_t textLength = ntohl(networkLength);
    if (textLength > MAX_MESSAGE_SIZE) {
        receiveBuffer.clear();
        return false;
    }

    size_t packetSize = PACKET_HEADER_SIZE + (size_t)textLength;
    if (receiveBuffer.size() < packetSize) {
        return false;
    }

    packet.data.assign((char*)(receiveBuffer.data() + PACKET_HEADER_SIZE), textLength);

    receiveBuffer.erase( receiveBuffer.begin(), receiveBuffer.begin() + packetSize);

    return true;
}


void WorkerThread::start_server(Server* server) {
    running = true;
    workerThread = std::thread(server_worker, server, this);
}

void WorkerThread::start_client(Client* client) {
    running = true;
    workerThread = std::thread(client_worker, client, this);
}

void WorkerThread::addSocket(SOCKET _socket) {
    std::lock_guard<std::mutex> lock(socketsMutex);
    sockets.push_back(new SocketData{_socket, {}, {}});
}

void WorkerThread::removeSocket(SOCKET _socket) {
    std::lock_guard<std::mutex> lock(socketsMutex);
    for (auto it = sockets.begin(); it != sockets.end(); ++it) {
        if ((*it)->_socket == _socket) {
            delete *it;
            sockets.erase(it);
            break;
        }
    }
}

void WorkerThread::stop() {
    running = false;
    if (workerThread.joinable()) {
        workerThread.join();
    }
}

bool WorkerThread::isRunning() const {
    return running;
}

std::vector<SocketData*> WorkerThread::getSockets() {
    std::lock_guard<std::mutex> lock(socketsMutex);
    return sockets;
}

void WorkerThread::enqueueMessage(SOCKET sourceSocket, PacketType type, const std::string& message) {
    std::lock_guard<std::mutex> lock(socketsMutex);
    Packet packet{type, message};
    std::string data = encodePacket(packet);

    for (SocketData* socketData : sockets) {
        if (socketData->_socket == sourceSocket) {
            std::lock_guard<std::mutex> queueLock(socketData->queueMutex);
            socketData->messageQueue.push({{type, message}, data, 0});
            printf("Enqueued message for socket %llu: %s\n",
                static_cast<unsigned long long>(socketData->_socket),
                message.c_str());
            break;
        }
    }
}

NetEvent::NetEvent(Type eventType, SOCKET eventSocket, Packet eventPacket)
    : type(eventType), _socket(eventSocket), packet(eventPacket) {}

Server::Server(PCSTR port) : port(port) {}

bool Server::init() {
    running = true;

#ifdef _WIN32
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed with error: %d\n", iResult);
        return false;
    }
#endif

    for (int i = 0; i < WORKER_THREAD_COUNT; ++i) {
        WorkerThread* workerThread = new WorkerThread();
        workerThread->start_server(this);
        workerThreads.push_back(workerThread);
    }

    struct addrinfo* result = NULL;
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    // Resolve the server address and port
    iResult = getaddrinfo(NULL, port, &hints, &result);
    if (iResult != 0) {
        printf("getaddrinfo failed with error: %d\n", iResult);
#ifdef _WIN32
        WSACleanup();
#endif
        return false;
    }

    // Create a SOCKET for the server to listen for client connections.
    ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (ListenSocket == INVALID_SOCKET) {
        printf("socket failed with error: %d\n", get_socket_error());
        freeaddrinfo(result);
#ifdef _WIN32
        WSACleanup();
#endif
        return false;
    }

    if (!set_non_blocking(ListenSocket)) {
        printf("Failed to set non-blocking mode for listen socket with error: %d\n", get_socket_error());
        close_socket(ListenSocket);
        ListenSocket = INVALID_SOCKET;
        freeaddrinfo(result);
        return false;
    }

    // Setup the TCP listening socket
    iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        printf("bind failed with error: %d\n", get_socket_error());
        freeaddrinfo(result);
        close_socket(ListenSocket);
        ListenSocket = INVALID_SOCKET;
#ifdef _WIN32
        WSACleanup();
#endif
        return false;
    }

    freeaddrinfo(result);

    iResult = listen(ListenSocket, SOMAXCONN);
    if (iResult == SOCKET_ERROR) {
        printf("listen failed with error: %d\n", get_socket_error());
        close_socket(ListenSocket);
        ListenSocket = INVALID_SOCKET;
#ifdef _WIN32
        WSACleanup();
#endif
        return false;
    }

    return true;
}

void Server::update() {
    // Accept a client socket
    SOCKET Socket = accept(ListenSocket, NULL, NULL);
    if (Socket == INVALID_SOCKET) {
        int error = get_socket_error();
        if (is_would_block(error)) {
            return;
        }

        printf("accept failed with error: %d\n", error);
        return;
    }

    if (!set_non_blocking(Socket)) {
        printf("Failed to set non-blocking mode for client socket with error: %d\n", get_socket_error());
        close_socket(Socket);
        return;
    }

    workerThreads[nextWorkerThreadIndex]->addSocket(Socket);
    nextWorkerThreadIndex = (nextWorkerThreadIndex + 1) % WORKER_THREAD_COUNT;
    pushEvent(NetEvent(NetEvent::Accepted, Socket));
}

void Server::sendMessage(SOCKET _socket, PacketType type, const std::string& message) {
    for (WorkerThread* thread : workerThreads) {
        std::vector<SocketData*> sockets = thread->getSockets();
        if (sockets.empty()) {
            continue;
        }

        for (SocketData* socketData : sockets) {
            SOCKET socket = socketData->_socket;
            if (_socket == socket) {
                thread->enqueueMessage(_socket, type, message);
                return;
            }
        }
    }
}

void Server::pollEvents(NetEvent* event) {
    std::lock_guard<std::mutex> lock(eventQueueMutex);
    if (!eventQueue.empty()) {
        *event = eventQueue.front();
        eventQueue.pop();
    }
}

void Server::pushEvent(NetEvent event) {
    std::lock_guard<std::mutex> lock(eventQueueMutex);
    eventQueue.push(event);
}

Server::~Server() {
    for (WorkerThread* workerThread : workerThreads) {
        workerThread->stop();
        delete workerThread;
    }
    workerThreads.clear();

    if (ListenSocket != INVALID_SOCKET) {
        close_socket(ListenSocket);
        ListenSocket = INVALID_SOCKET;
    }

#ifdef _WIN32
    WSACleanup();
#endif
}

bool Client::init() {
#ifdef _WIN32
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed with error: %d\n", iResult);
        return false;
    }
#endif

    return true;
}

bool Client::connectToServer(PCSTR address, PCSTR port) {
    struct addrinfo* result = NULL;
    struct addrinfo* ptr = NULL;
    struct addrinfo hints;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    int iResult = getaddrinfo(address, port, &hints, &result);
    if (iResult != 0) {
        printf("getaddrinfo failed with error: %d\n", iResult);
        return false;
    }

    // Attempt to connect to an address until one succeeds
    for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {
        clientSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
        if (clientSocket == INVALID_SOCKET) {
            continue;
        }

        iResult = connect(clientSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
        if (iResult == SOCKET_ERROR) {
            close_socket(clientSocket);
            clientSocket = INVALID_SOCKET;
            continue;
        }

        break;
    }

    freeaddrinfo(result);

    if (clientSocket == INVALID_SOCKET) {
        printf("Unable to connect to server. Error: %d\n", get_socket_error());
        return false;
    }

    if (!set_non_blocking(clientSocket)) {
        printf("Failed to set non-blocking mode for client socket with error: %d\n", get_socket_error());
        close_socket(clientSocket);
        clientSocket = INVALID_SOCKET;
        return false;
    }

    setConnected(true);
    pushEvent(NetEvent(NetEvent::Connected, clientSocket));

    running = true;
    workerThread = new WorkerThread();
    workerThread->addSocket(clientSocket);
    workerThread->start_client(this);

    return true;
}

void Client::sendMessage(PacketType type, const std::string& message) {
    if (workerThread == nullptr || clientSocket == INVALID_SOCKET) {
        return;
    }

    workerThread->enqueueMessage(clientSocket, type, message);
}

bool Client::pollEvent(NetEvent& event) {
    std::lock_guard<std::mutex> lock(eventQueueMutex);
    if (eventQueue.empty()) {
        return false;
    }

    event = eventQueue.front();
    eventQueue.pop();
    return true;
}

void Client::pushEvent(NetEvent event) {
    std::lock_guard<std::mutex> lock(eventQueueMutex);
    eventQueue.push(event);
}

bool Client::isRunning() const {
    return running;
}

bool Client::isConnected() const {
    return connected;
}

void Client::setConnected(bool value) {
    connected = value;
}

Client::~Client() {
    running = false;
    connected = false;

    if (workerThread != nullptr) {
        workerThread->stop();
        delete workerThread;
        workerThread = nullptr;
    }

#ifdef _WIN32
    WSACleanup();
#endif
}

bool set_non_blocking(SOCKET _socket)
{
#ifdef _WIN32
    u_long mode = 1;
    return ioctlsocket(_socket, FIONBIO, &mode) == 0;
#elif __linux__
    int flags = fcntl(_socket, F_GETFL, 0);
    if (flags == -1) {
        return false;
    }

    return fcntl(_socket, F_SETFL, flags | O_NONBLOCK) != -1;
#endif
}

void server_worker(Server* server, WorkerThread* workerThread) {
    char recvbuf[DEFAULT_BUFLEN];
    int recvbuflen = DEFAULT_BUFLEN;
    int iResult;
    fd_set readSet;
    fd_set writeSet;
    std::vector<SocketData*> sockets;
    std::queue<SendMessageData>* messageQueue;
    printf("Worker %p thread started\n", workerThread);

    // Receive until the peer shuts down the connection
    while (workerThread->isRunning()) {
        sockets = workerThread->getSockets();
        if (sockets.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        FD_ZERO(&readSet);
        FD_ZERO(&writeSet);

        SOCKET maxSocket = INVALID_SOCKET;

        for (SocketData* socketData : sockets) {
            SOCKET _socket = socketData->_socket;
            FD_SET(_socket, &readSet);

            {
                std::lock_guard<std::mutex> lock(socketData->queueMutex);
                if (!socketData->messageQueue.empty()) {
                    FD_SET(_socket, &writeSet);
                }
            }

            if (_socket > maxSocket) {
                maxSocket = _socket;
            }
        }

        timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 50 * 1000;

        iResult = select(get_select_nfds(maxSocket), &readSet, &writeSet, NULL, &timeout);
        if (iResult == SOCKET_ERROR) {
            printf("select failed with error: %d\n", get_socket_error());
            return;
        }

        if (iResult == 0) {
            // No sockets are ready for reading or writing
            continue;
        }

        for (int i = 0; i < (int)sockets.size(); ++i) {
            SocketData* socketData = sockets[i];
            SOCKET _socket = socketData->_socket;

            if (FD_ISSET(_socket, &readSet)) {
                iResult = recv(_socket, recvbuf, recvbuflen, 0);
                if (iResult > 0) {
                    socketData->receiveBuffer.insert(
                        socketData->receiveBuffer.end(),
                        (uint8_t*)recvbuf,
                        (uint8_t*)recvbuf + iResult
                    );
                    printf("Bytes received: %d\n", iResult);

                    Packet packet;
                    while (decodePacket(socketData->receiveBuffer, packet)) {
                        server->pushEvent(NetEvent(NetEvent::Received, _socket, packet));
                    }
                }
                else if (iResult == 0) {
                    printf("Connection closed by client socket %llu\n", static_cast<unsigned long long>(_socket));
                    close_socket(_socket);
                    workerThread->removeSocket(_socket);
                    server->pushEvent(NetEvent(NetEvent::Closed, _socket));
                }
                else {
                    int error = get_socket_error();
                    if (is_would_block(error)) {
                        continue;
                    }

                    printf("recv failed with error: %d\n", error);
                    close_socket(_socket);
                    workerThread->removeSocket(_socket);
                    server->pushEvent(NetEvent(NetEvent::Closed, _socket));
                }
            }

            if (FD_ISSET(_socket, &writeSet)) {
                std::lock_guard<std::mutex> lock(socketData->queueMutex);
                messageQueue = &socketData->messageQueue;

                while (!messageQueue->empty()) {
                    printf("Sending message to client socket %d\n", _socket);

                    SendMessageData& message = messageQueue->front();
                    size_t bufferSize = message.data.size();
                    size_t bufferSent = message.sentBytes;
                    size_t remaining = bufferSize - bufferSent;

                    int sendResult = send(
                        _socket,
                        message.data.c_str() + bufferSent,
                        (int)remaining,
                        get_send_flags()
                    );

                    if (sendResult == SOCKET_ERROR) {
                        int error = get_socket_error();
                        if (is_would_block(error)) {
                            break;
                        }

                        printf("send failed with error: %d\n", error);
                        server->pushEvent(NetEvent(NetEvent::Error, _socket));
                        break;
                    }
                    else if (sendResult == 0) {
                        printf("Connection closed by client socket %d\n", _socket);
                        server->pushEvent(NetEvent(NetEvent::Closed, _socket));
                        break;
                    }
                    else {
                        message.sentBytes += sendResult;

                        if (message.sentBytes >= bufferSize) {
                            Packet sentPacket = message.packet;
                            server->pushEvent(NetEvent(NetEvent::Sent, _socket, sentPacket));
                            messageQueue->pop();
                        }

                        printf("Sent %d bytes to client _socket %d\n", sendResult, _socket);
                    }
                }
            }
        }
    }

    // shutdown the connection since we're done
    for (SocketData* socketData : workerThread->getSockets()) {
        SOCKET _socket = socketData->_socket;
        iResult = shutdown(_socket, get_shutdown());
        if (iResult == SOCKET_ERROR) {
            printf("shutdown failed with error: %d\n", get_socket_error());
            close_socket(_socket);
            return;
        }

        close_socket(_socket);
    }
}

void client_worker(Client* client, WorkerThread* workerThread) {
    char recvbuf[DEFAULT_BUFLEN];
    int recvbuflen = DEFAULT_BUFLEN;
    int iResult;
    fd_set readSet;
    fd_set writeSet;
    std::vector<SocketData*> sockets;
    std::queue<SendMessageData>* messageQueue;
    printf("Worker %p thread started\n", (void*)workerThread);

    // Receive until the peer shuts down the connection
    while (workerThread->isRunning()) {
        sockets = workerThread->getSockets();
        if (sockets.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        FD_ZERO(&readSet);
        FD_ZERO(&writeSet);

        SOCKET maxSocket = INVALID_SOCKET;

        for (SocketData* socketData : sockets) {
            SOCKET _socket = socketData->_socket;
            FD_SET(_socket, &readSet);

            {
                std::lock_guard<std::mutex> lock(socketData->queueMutex);
                if (!socketData->messageQueue.empty()) {
                    FD_SET(_socket, &writeSet);
                }
            }

            if (_socket > maxSocket) {
                maxSocket = _socket;
            }
        }

        timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 50 * 1000;

        iResult = select(get_select_nfds(maxSocket), &readSet, &writeSet, NULL, &timeout);
        if (iResult == SOCKET_ERROR) {
            printf("select failed with error: %d\n", get_socket_error());
            return;
        }

        if (iResult == 0) {
            // No sockets are ready for reading or writing
            continue;
        }

        for (int i = 0; i < (int)sockets.size(); ++i) {
            SocketData* socketData = sockets[i];
            SOCKET _socket = socketData->_socket;
            bool socketClosed = false;

            if (FD_ISSET(_socket, &readSet)) {
                iResult = recv(_socket, recvbuf, recvbuflen, 0);
                if (iResult > 0) {
                    socketData->receiveBuffer.insert(
                        socketData->receiveBuffer.end(),
                        (uint8_t*)recvbuf,
                        (uint8_t*)recvbuf + iResult
                    );
                    printf("Bytes received: %d\n", iResult);

                    Packet packet;
                    while (decodePacket(socketData->receiveBuffer, packet)) {
                        client->pushEvent(NetEvent(NetEvent::Received, _socket, packet));
                    }
                }
                else if (iResult == 0) {
                    printf("Connection closed by client socket %llu\n", static_cast<unsigned long long>(_socket));
                    close_socket(_socket);
                    workerThread->removeSocket(_socket);
                    client->pushEvent(NetEvent(NetEvent::Closed, _socket));
                }
                else {
                    int error = get_socket_error();
                    if (is_would_block(error)) {
                        continue;
                    }

                    printf("recv failed with error: %d\n", error);
                    close_socket(_socket);
                    workerThread->removeSocket(_socket);
                    client->pushEvent(NetEvent(NetEvent::Closed, _socket));
                }
            }

            if (FD_ISSET(_socket, &writeSet)) {
                std::lock_guard<std::mutex> lock(socketData->queueMutex);
                messageQueue = &socketData->messageQueue;

                while (!messageQueue->empty()) {
                    printf("Sending message to client socket %d\n", _socket);

                    SendMessageData& message = messageQueue->front();
                    size_t bufferSize = message.data.size();
                    size_t bufferSent = message.sentBytes;
                    size_t remaining = bufferSize - bufferSent;

                    int sendResult = send(
                        _socket,
                        message.data.c_str() + bufferSent,
                        (int)remaining,
                        get_send_flags()
                    );

                    if (sendResult == SOCKET_ERROR) {
                        int error = get_socket_error();
                        if (is_would_block(error)) {
                            break;
                        }

                        printf("send failed with error: %d\n", error);
                        client->pushEvent(NetEvent(NetEvent::Error, _socket));
                        break;
                    }
                    else if (sendResult == 0) {
                        printf("Connection closed by client socket %d\n", _socket);
                        client->pushEvent(NetEvent(NetEvent::Closed, _socket));
                        break;
                    }
                    else {
                        message.sentBytes += sendResult;

                        if (message.sentBytes >= bufferSize) {
                            Packet sentPacket = message.packet;
                            client->pushEvent(NetEvent(NetEvent::Sent, _socket, sentPacket));
                            messageQueue->pop();
                        }

                        printf("Sent %d bytes to client _socket %d\n", sendResult, _socket);
                    }
                }
            }

            if (socketClosed) {
                continue;
            }
        }
    }

    // shutdown the connection since we're done
    for (SocketData* socketData : workerThread->getSockets()) {
        SOCKET _socket = socketData->_socket;
        iResult = shutdown(_socket, get_shutdown());
        if (iResult == SOCKET_ERROR) {
            printf("shutdown failed with error: %d\n", get_socket_error());
            close_socket(_socket);
            return;
        }

        close_socket(_socket);
    }
}
