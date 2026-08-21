#include <iostream>
#include <thread>
#include <chrono>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <fcntl.h>

#include "Engine.h"

// --- BACKGROUND SERVER THREAD ---
void RunServerDaemon() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8080);

    bind(server_fd, (struct sockaddr*)&address, sizeof(address));
    listen(server_fd, SOMAXCONN);

    // Make server socket non-blocking for epoll
    int flags = fcntl(server_fd, F_GETFL, 0);
    fcntl(server_fd, F_SETFL, flags | O_NONBLOCK);

    int epoll_fd = epoll_create1(0);
    epoll_event ev{}, events[10];
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = server_fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev);

    Engine engine(1000); // Initialize your high-performance matching engine

    // Silent background loop processing connections
    while (true) {
        int nfds = epoll_wait(epoll_fd, events, 10, 10);
        for (int i = 0; i < nfds; ++i) {
            if (events[i].data.fd == server_fd) {
                sockaddr_in client_addr{};
                socklen_t client_len = sizeof(client_addr);
                int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
                
                int c_flags = fcntl(client_fd, F_GETFL, 0);
                fcntl(client_fd, F_SETFL, c_flags | O_NONBLOCK);

                ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
                ev.data.fd = client_fd;
                epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev);
            } else {
                int client_fd = events[i].data.fd;
                Order incomingOrder;
                ssize_t bytes_read = read(client_fd, &incomingOrder, sizeof(Order));
                
                if (bytes_read > 0) {
                    auto trades = engine.ProcessOrder(incomingOrder);
                    for (const auto& trade : trades) {
                        TradeEvent event{trade.makerOrderId, trade.takerOrderId, trade.price, trade.quantity};
                        send(client_fd, &event, sizeof(TradeEvent), 0);
                    }
                } else {
                    close(client_fd);
                }
                
                ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
                ev.data.fd = client_fd;
                epoll_ctl(epoll_fd, EPOLL_CTL_MOD, client_fd, &ev);
            }
        }
    }
}

// --- FOREGROUND CLIENT LOOP ---
int main() {
    // 1. Launch the server daemon in a background thread
    std::thread serverThread(RunServerDaemon);
    serverThread.detach();

    // Give the background server a brief moment to bind to port 8080
    usleep(100000);

    // 2. Connect the local client to the background server over localhost TCP
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in serv_addr{};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cerr << "Local connection failed.\n";
        return -1;
    }

    std::cout << "========================================\n";
    std::cout << "    UNIFIED MATCHING ENGINE NODE        \n";
    std::cout << "========================================\n";
    std::cout << "Server daemon running on background thread.\n";
    std::cout << "Format: [Side (0=Buy, 1=Sell)] [Type (0=Limit, 1=Market)] [Price in Cents] [Quantity]\n";
    std::cout << "Example: 1 0 10500 10  (Limit sell: 10 units @ $105.00)\n";
    std::cout << "Example: 0 1 0 4       (Market buy: up to 4 units at available asks)\n\n";

    uint64_t local_order_counter = 1;
    int sideInput;
    int typeInput;
    uint64_t price;
    uint32_t qty;

    while (true) {
        std::cout << "Order> ";
        if (!(std::cin >> sideInput >> typeInput >> price >> qty)) break;

        if ((sideInput != 0 && sideInput != 1) || (typeInput != 0 && typeInput != 1)) {
            std::cout << "Invalid side or type.\n";
            continue;
        }

        auto now = std::chrono::high_resolution_clock::now();
        uint64_t current_timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
            now.time_since_epoch()
        ).count();

        Order ord;
        ord.orderId = local_order_counter++;
        ord.side = static_cast<Side>(sideInput);
        ord.price = price;
        ord.quantity = qty;
        ord.timestamp = current_timestamp;
        ord.type = static_cast<OrderType>(typeInput);

        send(sock, &ord, sizeof(Order), 0);
        std::cout << "  [Dispatched] ID: " << ord.orderId << "\n";

        usleep(10000); // Brief wait for server response

        TradeEvent trade;
        while (true) {
            ssize_t bytes = recv(sock, &trade, sizeof(TradeEvent), MSG_DONTWAIT);
            if (bytes <= 0) break;
            
            std::cout << "  >>> [TRADE EXECUTED] Maker ID: " << trade.makerOrderId 
                      << " | Taker ID: " << trade.takerOrderId
                      << " | Price: $" << (trade.price / 100.0) 
                      << " | Qty: " << trade.quantity << "\n";
        }
    }

    close(sock);
    return 0;
}
