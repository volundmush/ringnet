//
// Created by volund on 11/13/21.
//

#include <iostream>
#include <filesystem>
#include <fstream>
#include "ringnet/net.h"

bool copyover = false;

std::filesystem::path cpath("copyover.json");

void announce_ready(int conn_id) {
    std::cout << "Connection " << conn_id << " ready." << std::endl;
}

void close_cb(int conn_id) {
    std::cout << "Connection " << conn_id << " closed." << std::endl;
}

void receive_cb(int conn_id) {
    auto conn = ring::net::manager.connections.at(conn_id);
    auto data = conn->queue_in.front();
    std::cout << data << std::endl;
    conn->queue_in.pop_front();
    ring::net::manager.copyover();
}

void copyover_cb(nlohmann::json& j) {
    std::cout << "initializing copyover" << std::endl;
    std::cout << j << std::endl;
    std::ofstream of(cpath.string());
    of << j << std::endl;
    of.close();
    copyover = true;
}

void copyover_recover(nlohmann::json& j) {
    std::cout << "DID IT WORK?" << std::endl;
    for(const auto &l : ring::net::manager.plain_telnet_listeners) {
        std::cout << "PLAIN TELNET " << l.first << ": " << l.second->acceptor.native_handle() << std::endl;
    }
}

int main(int argc, char **argv) {
    ring::net::on_ready_cb = announce_ready;
    ring::net::on_close_cb = close_cb;
    ring::net::on_receive_cb = receive_cb;
    ring::net::copyover_prepare_cb = copyover_cb;
    ring::net::copyover_recover_cb = copyover_recover;

    bool copyover_recovered = false;

    if(std::filesystem::exists(cpath)) {
        std::ifstream jf(cpath.string());
        nlohmann::json j;
        jf >> j;
        jf.close();
        ring::net::manager.copyoverRecover(j);
        copyover_recovered = true;
    } else {
        if(!ring::net::manager.listenPlainTelnet("0.0.0.0", 2008)) {
            std::cout << "Error! Cannot bind to socket!" << std::endl;
            exit(1);
        }
    }
    if(copyover_recovered) {
        std::cout << "Recovered from copyover!" << std::endl;
        remove(cpath.string().c_str());
    }


    ring::net::manager.run();

    if(copyover) {
        std::cout << "Attempting copyover!" << std::endl;
        execl("./ringnet_test", "ringnet_test", nullptr);
    } else {
        std::cout << "okay we done!" << std::endl;
    }


}