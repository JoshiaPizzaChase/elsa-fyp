#ifndef TRANSPORT_MESSAGING_H
#define TRANSPORT_MESSAGING_H

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace transport {


    // to Json 
    // from Json
    // send over network
    // receive from network
    // use asio 
    
    void toJson();
    void fromJson();
    void send();
    void receive();




}

#endif // TRANSPORT_MESSAGING_H
