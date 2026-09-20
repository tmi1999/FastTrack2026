#include "net.h"

int main()
{
    Server server;
    if (!server.init()) {
        printf("Failed to initialize server.\n");
        return 1;
    }

    bool sentHello = false;

    do {
        SOCKET _socket = INVALID_SOCKET;
        NetEvent event(NetEvent::Type::Null, INVALID_SOCKET);
        server.update();
        server.pollEvents(&event);
        if (event.type == NetEvent::Type::Null) continue;

        switch (event.type)
        {
        case NetEvent::Type::Accepted:
            printf("Accepted\n");
            _socket = event._socket;
            break;
        case NetEvent::Type::Received:
            printf( "Received from client: %s\n", event.message.c_str());
            break;

        case NetEvent::Type::Sent:
            printf("Sent to client: %s\n", event.message.c_str());
            break;

        case NetEvent::Type::Closed:
            printf("Client closed connection.\n");
            break;

        case NetEvent::Type::Error:
            printf("Network error: %s\n", event.message.c_str());
            break;

        default:
            break;
        }

        if (_socket != INVALID_SOCKET) {
            printf("Sending Hello\n");
            server.sendMessage(_socket, "Hello from server");
        }
    } while (true);
    
    return 0;
}