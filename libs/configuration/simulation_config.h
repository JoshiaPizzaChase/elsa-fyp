#pragma once

#include <string>
#include <vector>

namespace simulation {
    struct NoiseTradersConfig {
        const std::string cfg_prefix{"noise_trader"};
        const int num_noise_traders{5};
        const double lambda_eps{20.0};
        const double bernoulli{0.5};
        const double pareto_scale{1.0};
        const double pareto_shape{2.0};
        const std::vector<std::string> tickers{"AAPL", "MSFT", "GOOG"};
    };
}
