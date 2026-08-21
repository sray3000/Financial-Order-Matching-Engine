#include <iostream>
#include <vector>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>

#include "Engine.h"

#define PORT 8080
#define MAX_EVENTS 64
#define READ_BUFFER_SIZE sizeof(Order)

// Helper to set a socket to non-blocking mode (required for epoll Edge-Triggered mode)
void SetNonBlocking(int sock) {
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
}

int main() {
    // 1. Initialize the Matching Engine with a 1,000,000 order pool capacity
    Engine engine(1000000);

    // 2. Create the listening TCP socket using raw POSIX system calls
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        std::cerr << "ERROR: Failed to create socket\n";
        return 1;
    }

    // Allow immediate reuse of the port after server restart
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 3. Bind socket to IP and Port
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; // Listen on all network interfaces
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "ERROR: Failed to bind socket to port " << PORT << "\n";
        close(server_fd);
        return 1;
    }

    // 4. Start listening for incoming connections
    if (listen(server_fd, SOMAXCONN) < 0) {
        std::cerr << "ERROR: Listen failed\n";
        close(server_fd);
        return 1;
    }

    SetNonBlocking(server_fd);

    // 5. Create the epoll instance for high-throughput event notification
    int epoll_fd = epoll_create1(0);
    if (epoll_fd < 0) {
        std::cerr << "ERROR: Failed to create epoll descriptor\n";
        close(server_fd);
        return 1;
    }

    // Register the server socket to epoll
    epoll_event ev{};
    ev.events = EPOLLIN | EPOLLET; // Input event, Edge-Triggered mode
    ev.data.fd = server_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev) < 0) {
        std::cerr << "ERROR: Failed to add server socket to epoll\n";
        close(server_fd);
        close(epoll_fd);
        return 1;
    }

    std::cout << "=== Low-Latency POSIX Matching Engine Server Active ===" << std::endl;
    std::cout << "Listening for raw binary TCP packets on port " << PORT << "...\n";

    std::vector<epoll_event> events(MAX_EVENTS);

    // 6. The Main Server Event Loop
    while (true) {
        int nfds = epoll_wait(epoll_fd, events.data(), MAX_EVENTS, -1);
        if (nfds < 0) {
            std::cerr << "ERROR: epoll_wait failure\n";
            break;
        }

        for (int i = 0; i < nfds; ++i) {
            int current_fd = events[i].data.fd;

            // Case A: New incoming client connection
            if (current_fd == server_fd) {
                sockaddr_in client_addr;
                socklen_t client_len = sizeof(client_addr);
                
                int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
                if (client_fd < 0) {
                    continue;
                }

                SetNonBlocking(client_fd);

                // Add new client socket to epoll monitoring
                ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
                ev.data.fd = client_fd;
                epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev);
            } 
            // Case B: Data incoming from an active client (a raw Order struct)
            else {
                Order incomingOrder;
                ssize_t bytesRead = recv(current_fd, &incomingOrder, sizeof(Order), 0);

                if (bytesRead <= 0) {
                    // Client disconnected or error occurred
                    close(current_fd);
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, current_fd, nullptr);
                } else if (bytesRead == sizeof(Order)) {
                    // ZERO-COPY DESERIALIZATION: Pass binary struct straight into the core engine!
                    auto trades = engine.ProcessOrder(incomingOrder);

                    // Send any resulting trade execution confirmations back to the client over TCP
                    for (const auto& trade : trades) {
                        send(current_fd, &trade, sizeof(TradeEvent), MSG_DONTWAIT);
                    }
                }

                // Re-arm the socket for future reads (due to EPOLLONESHOT)
                ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
                ev.data.fd = current_fd;
                epoll_ctl(epoll_fd, EPOLL_CTL_MOD, current_fd, &ev);
            }
        }
    }

    close(server_fd);
    close(epoll_fd);
    return 0;
}