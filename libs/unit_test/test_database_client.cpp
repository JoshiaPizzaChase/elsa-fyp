#include <questdb/ingress/line_sender.hpp>


int main()
{
    auto sender = questdb::ingress::line_sender::from_conf(
        "http::addr=localhost:9000;");
    return 0;
}
