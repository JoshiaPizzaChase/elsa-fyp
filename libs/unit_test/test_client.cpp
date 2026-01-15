#include "transport/websocket_client.h"
#include <exception>
#include <print>

int main(int argc, char* argv[]) {
    
    if (argc != 2) {
        std::println("Correct use: ./{} <port num>", argv[0]);
        return 0;
    }

    transport::WebsocketManagerClient omc;
    std::string uri{"ws://localhost:"};
    uri += argv[1];

    try {
        omc.start();

        omc.connect(uri);

        while (omc.getMetadata(0)->getStatus() != "Open") {
        };

        while (true) {

            std::cout << "What to send to server?: ";

            std::string input;
            std::getline(std::cin, input);

            omc.send(0, input);
        }

    } catch (const std::exception& e) {
        std::println(std::cerr, "Caught exception: {}", e.what());
        return -1;
    }

    return 0;
}
