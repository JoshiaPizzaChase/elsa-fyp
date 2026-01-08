#ifndef ELSA_FYP_CLIENT_SDK_TEST_CLIENT_H
#define ELSA_FYP_CLIENT_SDK_TEST_CLIENT_H
#include "fix_client.h"

class TestClient : public FixClient {
  public:
    TestClient(const std::string& file) : FixClient(file) {};
    void on_order_cancel_rejected(const std::string& custom_order_id,
                                  const std::string& reason) override {
        std::cout << "[Test] Order Cancel Rejected: " << custom_order_id << " " << reason
                  << '\n';
    }
    void on_order_update(const ExecutionReport& report) override {
        std::cout << "[Test] Order Update" << '\n';
        std::cout << "Ticker: " << report.ticker << '\n';
        std::cout << "OrderId: " << report.order_id << '\n';
        std::cout << "CustomOrderId: " << report.custom_order_id << '\n';
        std::cout << "Filled" << report.filled_qty << '\n';
        std::cout << "Last Px" << report.last_px << '\n';
    }
};

#endif // ELSA_FYP_CLIENT_SDK_TEST_CLIENT_H
