#include "market_data_processor.h"

int main(int argc, char* argv[]) {
    auto mdp = MarketDataProcessor(9000);
    mdp.start();

    return 0;
}
