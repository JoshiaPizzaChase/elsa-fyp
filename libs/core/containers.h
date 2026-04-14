#pragma once

#include "orders.h"
#include <optional>
#include <string>
#include <variant>

namespace core {

struct NewOrderSingleContainer {
    std::string sender_comp_id;
    std::string target_comp_id;
    std::optional<int> order_id;
    int cl_ord_id;
    std::string symbol;
    // TODO: add account field to let one "broker" have many "comps"
    Side side;
    std::int32_t order_qty;
    OrderType ord_type;
    std::optional<std::int32_t>
        price; // Only required for limit orders. No price for market orders.
    TimeInForce time_in_force;
};

struct CancelOrderRequestContainer {
    std::string sender_comp_id;
    std::string target_comp_id;
    std::optional<int> order_id;
    int orig_cl_ord_id; // Original clOrdId for the order this cancel request is for.
    int cl_ord_id;
    std::string symbol;
    // TODO: add account field to let one "broker" have many "comps"
    Side side;
    std::int32_t order_qty;
};

struct ExecutionReportContainer {
    std::string sender_comp_id;
    std::string target_comp_id;
    int order_id;                      // Our std::int32_ternal ID for the order.
    int cl_order_id;                   // Client-defined.
    std::optional<int> orig_cl_ord_id; // Only required for response to cancel order requests.
    std::string exec_id;               // Unique ID for this execution report.
    ExecTransType exec_trans_type;     // Describes the type of execution report.
    ExecType exec_type;                // Order event that caused the issuance of this report.
    OrderStatus ord_status;            // Always the order status.
    std::optional<std::string>
        text; // Instead of using ord_reject_reason, which does not have the vals we want (e.g.
              // insufficient balance), we went with text instead.
    std::string symbol;
    Side side;
    std::optional<std::int32_t> price; // Only required in response to limit orders.
    TimeInForce time_in_force;
    std::int32_t leaves_qty;
    std::int32_t cum_qty;
    std::int32_t avg_px;
};

struct FillCostQueryContainer {
    std::string symbol;
    std::int32_t quantity;
    Side side;
};

struct FillCostResponseContainer {
    // Optional because Matching Engine may fail to retreive data.
    std::optional<std::int32_t> total_cost;
};

struct TradeContainer {
    std::string ticker{};
    int price{0};
    int quantity{0};
    std::string trade_id{};
    std::string taker_id{};
    std::string maker_id{};
    int taker_order_id{0};
    int maker_order_id{0};
    bool is_taker_buyer{false};
};

struct CancelOrderResponseContainer {
    int order_id;
    int cl_ord_id;
    bool success;
};

using Container = std::variant<core::NewOrderSingleContainer, core::CancelOrderRequestContainer,
                               core::ExecutionReportContainer, core::FillCostQueryContainer,
                               core::FillCostResponseContainer, core::TradeContainer,
                               core::CancelOrderResponseContainer>;

} // namespace core

template <>
struct std::formatter<core::NewOrderSingleContainer> {
    constexpr auto parse(std::format_parse_context& ctx) {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const core::NewOrderSingleContainer& nosc, FormatContext& ctx) const {
        return std::format_to(ctx.out(),
                              "NewOrderSingleContainer{{sender_comp_id: {}, target_comp_id: {}, "
                              "order_id: {}, cl_ord_id: {}, symbol: {}, side: {}, order_qty: {}, "
                              "ord_type: {}, price: {}, time_in_force: {}}}",
                              nosc.sender_comp_id, nosc.target_comp_id, nosc.order_id.value_or(-1),
                              nosc.cl_ord_id, nosc.symbol, nosc.side, nosc.order_qty, nosc.ord_type,
                              nosc.price.value_or(-1), nosc.time_in_force);
    }
};

template <>
struct std::formatter<core::TradeContainer> {
    constexpr auto parse(std::format_parse_context& ctx) {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const core::TradeContainer& tc, FormatContext& ctx) const {
        return std::format_to(ctx.out(),
                              "TradeContainer{{ticker: {}, price: {}, quantity: {}, trade_id: {}, "
                              "taker_id: {}, maker_id: {}, taker_order_id: {}, maker_order_id: {}, "
                              "is_taker_buyer: {}}}",
                              tc.ticker, tc.price, tc.quantity, tc.trade_id, tc.taker_id,
                              tc.maker_id, tc.taker_order_id, tc.maker_order_id, tc.is_taker_buyer);
    }
};

template <>
struct std::formatter<core::CancelOrderRequestContainer> {
    constexpr auto parse(std::format_parse_context& ctx) {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const core::CancelOrderRequestContainer& corc, FormatContext& ctx) const {
        return std::format_to(
            ctx.out(),
            "CancelOrderRequestContainer{{sender_comp_id: {}, target_comp_id: {}, "
            "order_id: {}, orig_cl_ord_id: {}, cl_ord_id: {}, symbol: {}, side: {}, "
            "order_qty: {}}}",
            corc.sender_comp_id, corc.target_comp_id, corc.order_id.value_or(-1),
            corc.orig_cl_ord_id, corc.cl_ord_id, corc.symbol, corc.side, corc.order_qty);
    }
};

template <>
struct std::formatter<core::CancelOrderResponseContainer> {
    constexpr auto parse(std::format_parse_context& ctx) {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const core::CancelOrderResponseContainer& corc, FormatContext& ctx) const {
        return std::format_to(
            ctx.out(), "CancelOrderResponseContainer{{order_id: {}, cl_ord_id: {}, success: {}}}",
            corc.order_id, corc.cl_ord_id, corc.success);
    }
};

template <>
struct std::formatter<core::FillCostQueryContainer> {
    constexpr auto parse(std::format_parse_context& ctx) {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const core::FillCostQueryContainer& fcqc, FormatContext& ctx) const {
        return std::format_to(ctx.out(),
                              "FillCostQueryContainer{{symbol: {}, quantity: {}, side: {}}}",
                              fcqc.symbol, fcqc.quantity, fcqc.side);
    }
};

template <>
struct std::formatter<core::FillCostResponseContainer> {
    constexpr auto parse(std::format_parse_context& ctx) {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const core::FillCostResponseContainer& fcrc, FormatContext& ctx) const {
        return std::format_to(ctx.out(), "FillCostResponseContainer{{total_cost: {}}}",
                              fcrc.total_cost.value_or(-1));
    }
};

template <>
struct std::formatter<core::ExecutionReportContainer> {
    constexpr auto parse(std::format_parse_context& ctx) {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const core::ExecutionReportContainer& erc, FormatContext& ctx) const {
        return std::format_to(
            ctx.out(),
            "ExecutionReportContainer{{sender_comp_id: {}, target_comp_id: {}, order_id: {}, "
            "cl_order_id: {}, orig_cl_ord_id: {}, exec_id: {}, exec_trans_type: {}, exec_type: {}, "
            "ord_status: {}, text: {}, symbol: {}, side: {}, price: {}, time_in_force: {}, "
            "leaves_qty: {}, cum_qty: {}, avg_px: {}}}",
            erc.sender_comp_id, erc.target_comp_id, erc.order_id, erc.cl_order_id,
            erc.orig_cl_ord_id.value_or(-1), erc.exec_id, erc.exec_trans_type, erc.exec_type,
            erc.ord_status, erc.text.value_or("N/A"), erc.symbol, erc.side, erc.price.value_or(-1),
            erc.time_in_force, erc.leaves_qty, erc.cum_qty, erc.avg_px);
    }
};
