#include<iostream>
#include<OrderBook.cc>

class App {
public:    
    OrderBook<std::greater<long long>> bids;
    OrderBook<std::less<long long>> asks;

    App() {

    };

    void PrintBook() {
        std::cout << "\t\tBIDS\t\t\t\tASKS\t\t\n";

    };
};

int main()
  {
    App app;
    
  }