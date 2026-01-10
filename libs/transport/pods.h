#ifndef TRANSPORT_MESSAGES_H
#define TRANSPORT_MESSAGES_H

#include <variant>

/*
 * POD stands for plain old data,
 * i.e. binary serialization to send C++ structs over the network.
 */
namespace transport {

struct NewOrderRequestPod {};

struct CancelOrderRequestPod {};

struct ExecutionReportPod {};

using MessagePod = std::variant<NewOrderRequestPod, CancelOrderRequestPod, ExecutionReportPod>;

} // namespace transport

#endif // TRANSPORT_MESSAGES_H
