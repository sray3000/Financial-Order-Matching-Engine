#include <iostream>
#include <cstring>
#include <chrono>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

enum class Side : uint8_t { Buy = 0, Sell = 1 };

struct Order {
    uint64_t orderId;
    Side side;
    uint64_t price;
    uint32_t quantity;
    uint64_t timestamp;
};

struct TradeEvent {
    uint64_t makerOrderId;
    uint64_t takerOrderId;
    uint64_t price;
    uint32_t quantity;
};

int main() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        std::cerr << "Socket creation error\n";
        return -1;
    }

    sockaddr_in serv_addr{};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(8080);
    
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        std::cerr << "Invalid address\n";
        return -1;
    }

    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cerr << "Connection Failed. Make sure server.cc is running.\n";
        return -1;
    }

    std::cout << "=== Interactive Trading Client Connected ===\n";
    std::cout << "Format: [Side (0=Buy, 1=Sell)] [Price in Cents] [Quantity]\n";
    std::cout << "Example: 0 10500 10  (Buy 10 units @ $105.00)\n\n";

    uint64_t local_order_counter = 1; // System-managed auto-incrementing ID generator
    int sideInput;
    uint64_t price;
    uint32_t qty;

    while (true) {
        std::cout << "Order> ";
        if (!(std::cin >> sideInput >> price >> qty)) {
            break;
        }

        // 1. Automatically generate high-precision nanosecond timestamp
        auto now = std::chrono::high_resolution_clock::now();
        uint64_t current_timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
            now.time_since_epoch()
        ).count();

        Order ord;
        ord.orderId = local_order_counter++;                    // Auto-assigned unique ID
        ord.side = static_cast<Side>(sideInput);
        ord.price = price;
        ord.quantity = qty;
        ord.timestamp = current_timestamp;                      // Auto-assigned timestamp

        // Send binary order packet to server
        ssize_t sentBytes = send(sock, &ord, sizeof(Order), 0);
        if (sentBytes < 0) {
            std::cerr << "Failed to send order.\n";
            break;
        }

        std::cout << "  [Dispatched] ID: " << ord.orderId << " | Timestamp: " << ord.timestamp << " ns\n";

        // Brief pause to allow the server thread to process and stream back execution feedback
        usleep(10000);

        // Read and display any resulting trade executions returned by the server
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