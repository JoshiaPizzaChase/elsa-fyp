#include "websocket_client.h"
#include <iostream>

int main(int argc, char* argv[]) {
    transport::WebsocketManagerClient client;
    client.start();
    auto id = client.connect("ws://localhost:9001").value();
    std::cout << id << std::endl;
    while (true) {
        auto res = client.dequeue_message(id);
        if (res.has_value()) {
            std::cout << res.value() << std::endl;
        }
    }
    return 0;
}
