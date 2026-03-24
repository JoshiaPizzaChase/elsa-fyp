#pragma once
#include <expected>

enum Service {
    gateway = 0,
    oms = 1,
    me = 2,
    mdp = 3,
};

inline std::string service_enum_to_str(const Service& service) {
    switch (service) {
    case Service::gateway:
        return "gateway";
    case Service::oms:
        return "oms";
    case Service::me:
        return "me";
    case Service::mdp:
        return "mdp";
    }
    return "unknown";
}

inline std::expected<Service, std::string> service_str_to_enum(const std::string_view service_str) {
    if (service_str == "gateway") {
        return Service::gateway;
    } else if (service_str == "oms") {
        return Service::oms;
    } else if (service_str == "me") {
        return Service::me;
    } else if (service_str == "mdp") {
        return Service::mdp;
    }
    return std::unexpected{
        std::format("trying to convert unknown service str to enum: {}", service_str)};
}
