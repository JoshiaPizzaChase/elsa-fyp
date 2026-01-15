#include "transport/websocket_server.h"
#include <exception>
#include <string>

int main(int argc, char* argv[]) {

    if (argc != 2) {
        std::println("Correct use: ./{} <port num>", argv[0]);
        return 0;
    }
    const int port{std::stoi(argv[1])};
    transport::OrderManagerServer oms{port};

    try {
        oms.start();

        while (true) {
            std::cout << "p for poll from queue, others do nothing: ";
            char command;
            std::cin >> command;

            if (command == 'q') {
                auto msg = oms.dequeueMessage(0);
                if (msg.has_value()) {
                    std::println("Message: {}", *msg);
                }
            } else {
                continue;
            }
        }

    } catch (const std::exception& e) {
        std::println(std::cerr, "Caught exception: {}", e.what());
        return -1;
    }

    return 0;
}
