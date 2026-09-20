#include "net.h"

int main()
{
    Client client;
    if (!client.init()) {
        printf("Failed to initialize client.\n");
        return 1;
    }

    if (!client.connectToServer("127.0.0.1", DEFAULT_PORT)) {
        printf("Failed to connect to server.\n");
        return 1;
    }

    bool sentHello = false;
    bool running = true;

    while (running) {
        NetEvent event(NetEvent::Type::Null, INVALID_SOCKET);

        while (client.pollEvent(event)) {
            switch (event.type)
            {
            case NetEvent::Type::Connected:
                printf("Connected to server!\n");
                break;

            case NetEvent::Type::Received:
                printf( "Received from server: %s\n", event.message.c_str());
                break;

            case NetEvent::Type::Sent:
                printf("Sent to server: %s\n", event.message.c_str());
                break;

            case NetEvent::Type::Closed:
                printf("Server closed connection.\n");
                running = false;
                break;

            case NetEvent::Type::Error:
                printf("Network error: %s\n", event.message.c_str());
                running = false;
                break;

            default:
                break;
            }
        }

        if (client.isConnected() && !sentHello) {
            printf("Sending Hello!\n");
            client.sendMessage("Hello from client");
            sentHello = true;
        }
    }
    
    return 0;
}