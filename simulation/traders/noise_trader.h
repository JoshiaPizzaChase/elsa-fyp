#pragma once

#include <absl/strings/str_format.h>
#include <chrono>
#include <memory>
#include <order.h>
#include <random>
#include <thread>

// TODO: Switch to CRTP for compile-time polymorphism later.
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

class NoiseTrader {
  public:
    NoiseTrader(std::unique_ptr<ProcessGenerator> process_generator,
                std::unique_ptr<DecisionGenerator<OrderSide>> decision_generator)
        : m_process_generator{std::move(process_generator)},
          m_side_decision_generator{std::move(decision_generator)} {
    }

    void run_strategy() {
        while (true) {
            OrderSide buy_or_sell{m_side_decision_generator->generate()};


            m_process_generator->wait_for_arrival();
        }
    }

  private:
    OrderSide generate_random_buy_or_sell() {
        return m_side_decision_generator->generate();
    }

  private:
    std::unique_ptr<ProcessGenerator> m_process_generator;
    std::unique_ptr<DecisionGenerator<OrderSide>> m_side_decision_generator;
};
