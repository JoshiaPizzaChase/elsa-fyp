#include "transport/websocket_client.h"
#include <exception>
#include <print>
#include <unistd.h>

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
        std::cout << "Starting connection to " << uri << "\n";

        omc.connect(uri);
        std::cout << "Connecting...\n";

        while (omc.get_metadata(0)->get_status() != transport::ConnectionStatus::open) {
            // Wait until connection is open
            std::cout << ".";
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        };

        while (true) {

            std::cout << "What to send to server?: q to stop";

            std::string input;
            std::getline(std::cin, input);

            if (input == "q") {
                omc.stop();
                std::cout << "sleeping 5 seconds before ending program...";
                sleep(5);
                break;
            } else {
                omc.send(0, input);
            }
        }

    } catch (const std::exception& e) {
        std::println(std::cerr, "Caught exception: {}", e.what());
        return -1;
    }

    return 0;
}
