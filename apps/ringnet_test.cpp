//
// Created by volund on 11/13/21.
//

#include <iostream>
#include "ringnet/net.h"

void announce_ready(int conn_id) {
    std::cout << "An Entity is ready!" << std::endl;
}

void close_cb(int conn_id) {
    std::cout << "Connection " << conn_id << " closed." << std::endl;
}

void receive_cb(int conn_id) {
    auto conn = ring::net::manager.connections.at(conn_id);
    auto data = conn->queue_in.front();
    std::cout << data << std::endl;
}

int main(int argc, char **argv) {
    ring::net::on_ready_cb = announce_ready;
    ring::net::on_close_cb = close_cb;
    ring::net::on_receive_cb = receive_cb;

    if(ring::net::manager.listenPlainTelnet("0.0.0.0", 2008))
        ring::net::run();
}