#pragma once

#include <chrono>
#include <fix_client.h>
#include <cmath>
#include <memory>
#include <order.h>
#include <random>
#include <thread>
#include <functional>
#include <iostream>
#include "logger/logger.h"

// TODO 1: Switch to CRTP for compile-time polymorphism later.
// TODO 2: Improve the class designs below. Having a separate parent class for ProcessGenerator and
// DecisionGenerator and QuantityGenerator seems a bit dumb.
// TODO 3: Improve the NoiseTrader class. We might want a factory pattern such that the caller does
// not have to create the ProcessGenerator, DecisionGenerator, and QuantityGenerator themselves. Any
// other ideas..?
// TODO 4: Should the NoiseTrader class even own the ProcessGenerator, DecisionGenerator, and
// QuantityGenerator themselves? I'm scared it becomes a God class.
// TODO 5: Each generator having its own random device is large AF. We may want to share/pass into generator by the owning class.
// TODO 6: I used rand in run_strategy below, which might be slow.

namespace simulation {
class ProcessGenerator {
  public:
    virtual void wait_for_arrival() = 0;
    virtual ~ProcessGenerator() = default;
};

class PoissonProcessGenerator : public ProcessGenerator {
  public:
    PoissonProcessGenerator(double lambda_eps)
        : m_lambda_eps{lambda_eps}, m_rd{}, m_generator{m_rd()}, m_distribution{m_lambda_eps} {
    }

    void wait_for_arrival() override {
        const double wait_time{m_distribution(m_generator)};
        std::this_thread::sleep_for(std::chrono::duration<double>{wait_time});
    }

  private:
    double m_lambda_eps; // Intensity parameter, in events per second

    std::random_device m_rd;
    std::mt19937 m_generator;
    std::exponential_distribution<double> m_distribution;
};

template <typename DecisionType>
class DecisionGenerator {
  public:
    virtual DecisionType generate() = 0;
    virtual ~DecisionGenerator() = default;
};

template <typename BinaryDecisionType>
class BernoulliDecisionGenerator : public DecisionGenerator<BinaryDecisionType> {
  public:
    BernoulliDecisionGenerator(double probability = 0.5)
        : m_probability{probability}, m_rd{}, m_generator(m_rd()), m_distribution{m_probability} {
    }

    double get_probability() {
        return m_probability;
    }

    BinaryDecisionType generate() override {
        if constexpr (std::is_same_v<BinaryDecisionType, OrderSide>) {
            return m_distribution(m_generator) ? OrderSide::BUY : OrderSide::SELL;
        } else {
            return m_distribution(m_generator) ? static_cast<BinaryDecisionType>(0)
                                               : static_cast<BinaryDecisionType>(1);
        }
    }

  private:
    double m_probability;

    std::random_device m_rd;
    std::mt19937 m_generator;
    std::bernoulli_distribution m_distribution;
};

template <typename QuantityType>
class QuantityGenerator {
  public:
    virtual QuantityType generate() = 0;
    virtual ~QuantityGenerator() = default;
};

template <typename T>
class UniformQuantityGenerator : public QuantityGenerator<T> {
  public:
    UniformQuantityGenerator(T min_val, T max_val)
        : m_min{min_val}, m_max{max_val}, m_rd{}, m_generator(m_rd()),
          m_distribution{m_min, m_max} {
    }

    T generate() override {
        return m_distribution(m_generator);
    }

  private:
    T m_min;
    T m_max;

    std::random_device m_rd;
    std::mt19937 m_generator;
    std::uniform_real_distribution<T> m_distribution;
};

template <typename T>
class ParetoQuantityGenerator : public QuantityGenerator<T> {
  public:
    ParetoQuantityGenerator(T scale, T shape)
        : m_scale{scale}, m_shape{shape}, m_rd{}, m_generator(m_rd()),
          m_distribution{0.0, 1.0} {
    }

    T generate() override {
        T u = m_distribution(m_generator);
        // Inverse transform sampling for Pareto distribution
        return m_scale * std::pow(1.0 - u, -1.0 / m_shape);
    }

  private:
    T m_scale;
    T m_shape;

    std::random_device m_rd;
    std::mt19937 m_generator;
    std::uniform_real_distribution<T> m_distribution;
};

template <typename T>
class LogNormalQuantityGenerator : public QuantityGenerator<T> {
  public:
    LogNormalQuantityGenerator(T m, T s)
        : m_m{m}, m_s{s}, m_rd{}, m_generator(m_rd()),
          m_distribution{m_m, m_s} {
    }

    T generate() override {
        return m_distribution(m_generator);
    }

  private:
    T m_m; // Mean of the underlying normal distribution
    T m_s; // Standard deviation of the underlying normal distribution

    std::random_device m_rd;
    std::mt19937 m_generator;
    std::lognormal_distribution<T> m_distribution;
};

class NoiseTrader {
  public:
    NoiseTrader(std::unique_ptr<ProcessGenerator> process_generator,
                std::unique_ptr<DecisionGenerator<OrderSide>> side_generator,
                std::unique_ptr<QuantityGenerator<double>> quantity_generator,
                std::unique_ptr<FixClient> fix_client,
                std::vector<std::string> tickers,
                std::function<double(const std::string&)> baseline_price_fn = nullptr,
                std::shared_ptr<spdlog::logger> logger = nullptr)
        : m_process_generator{std::move(process_generator)},
          m_side_generator{std::move(side_generator)},
          m_quantity_generator{std::move(quantity_generator)}, m_fix_client{std::move(fix_client)},
          m_tickers{std::move(tickers)}, m_baseline_price_fn{std::move(baseline_price_fn)},
          m_logger{std::move(logger)} {
    }

    void run_strategy() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::normal_distribution<> price_noise(0.0, 5.0); // Random noise around baseline
        std::bernoulli_distribution is_limit_order(0.5);  // 50% chance for limit order

        while (true) {
            OrderSide buy_or_sell{m_side_generator->generate()};
            double quantity{m_quantity_generator->generate()};
            std::string ticker = m_tickers[rand() % m_tickers.size()];

            if (m_fix_client && m_fix_client->is_connected()) {
                if (is_limit_order(gen)) {
                    const double baseline_price = m_baseline_price_fn ? m_baseline_price_fn(ticker) : 100.0;
                    // Generate a random limit price around the latest observed baseline.
                    double price = baseline_price + price_noise(gen);
                    price = std::max(0.01, std::round(price * 100.0) / 100.0);

                    m_fix_client->submit_limit_order(ticker, price, quantity, buy_or_sell, TimeInForce::GTC, ++m_client_order_id);
                    if (m_logger) {
                        m_logger->info("[NoiseTrader] Submitted {} limit order for {} | Px: {} | Qty: {}",
                                       (buy_or_sell == OrderSide::BUY ? "BUY" : "SELL"), ticker,
                                       price, quantity);
                    }
                } else {
                    m_fix_client->submit_market_order(ticker, quantity, buy_or_sell, ++m_client_order_id);
                    if (m_logger) {
                        m_logger->info("[NoiseTrader] Submitted {} market order for {} | Qty: {}",
                                       (buy_or_sell == OrderSide::BUY ? "BUY" : "SELL"), ticker,
                                       quantity);
                    }
                }
            }

            m_process_generator->wait_for_arrival();
        }
    }

  private:
    std::unique_ptr<ProcessGenerator> m_process_generator;
    std::unique_ptr<DecisionGenerator<OrderSide>> m_side_generator;
    std::unique_ptr<QuantityGenerator<double>> m_quantity_generator;
    std::unique_ptr<FixClient> m_fix_client;
    std::vector<std::string> m_tickers;
    std::function<double(const std::string&)> m_baseline_price_fn;
    std::shared_ptr<spdlog::logger> m_logger;

    int m_client_order_id{0};
};
}
