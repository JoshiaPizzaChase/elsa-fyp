#pragma once

#include <chrono>
#include <fix_client.h>
#include <memory>
#include <order.h>
#include <random>
#include <thread>

// TODO 1: Switch to CRTP for compile-time polymorphism later.
// TODO 2: Improve the class designs below. Having a separate parent class for ProcessGenerator and DecisionGenerator and QuantityGenerator seems a bit dumb.
// TODO 3: Improve the NoiseTrader class. We might want a factory pattern such that the caller does not have to create the ProcessGenerator, DecisionGenerator, and QuantityGenerator themselves. Any other ideas..?
// TODO 4: Should the NoiseTrader class even own the ProcessGenerator, DecisionGenerator, and QuantityGenerator themselves? I'm scared it becomes a God class.
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
        return m_distribution(m_generator) ? static_cast<BinaryDecisionType>(0)
                                           : static_cast<BinaryDecisionType>(1);
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
        : m_min{min_val}, m_max{max_val}, m_rd{}, m_generator(m_rd()), m_distribution{m_min, m_max} {
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

class NoiseTrader {
  public:
    NoiseTrader(std::unique_ptr<ProcessGenerator> process_generator,
                std::unique_ptr<DecisionGenerator<OrderSide>> side_generator,
                std::unique_ptr<QuantityGenerator<double>> quantity_generator,
                std::unique_ptr<FixClient> fix_client,
                std::string ticker)
        : m_process_generator{std::move(process_generator)},
          m_side_generator{std::move(decision_generator)},
          m_quantity_generator{std::move(quantity_generator)},
          m_fix_client{std::move(fix_client)},
          m_ticker{std::move(ticker)} {
    }

    void run_strategy() {
        while (true) {
            OrderSide buy_or_sell{m_side_generator->generate()};
            double quantity{m_quantity_generator->generate()};

            if (m_fix_client && m_fix_client->is_connected()) {
                m_fix_client->submit_market_order(m_ticker, quantity, buy_or_sell, ++m_client_order_id);
            }

            m_process_generator->wait_for_arrival();
        }
    }

  private:
    std::unique_ptr<ProcessGenerator> m_process_generator;
    std::unique_ptr<DecisionGenerator<OrderSide>> m_side_generator;
    std::unique_ptr<QuantityGenerator<double>> m_quantity_generator;
    std::unique_ptr<FixClient> m_fix_client;
    std::string m_ticker;
    int m_client_order_id{0};
};
