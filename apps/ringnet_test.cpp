//
// Created by volund on 11/13/21.
//

#include <iostream>
#include <filesystem>
#include <fstream>
#include "ringnet/net.h"

bool copyover = false;

std::filesystem::path cpath("copyover.json");


void copyover_cb() {
    auto j = ring::net::manager.copyover();
    std::cout << "initializing copyover" << std::endl;
    std::cout << j << std::endl;
    std::ofstream of(cpath.string());
    of << j << std::endl;
    of.close();
}


void copyover_recover() {
    std::cout << "DID IT WORK?" << std::endl;
    for(const auto &l : ring::net::manager.plain_telnet_listeners) {
        std::cout << "PLAIN TELNET " << l.first << ": " << l.second->acceptor.native_handle() << std::endl;
    }
}

std::unordered_map<std::string, std::weak_ptr<ring::net::MudConnection>> conns;



void check_status(boost::system::error_code ec, boost::asio::steady_timer &timer) {
    if(ec) std::cout << "Got an error: " << ec << std::endl;

    ring::net::ConnectionMsg *m;
    if(ring::net::manager.events.pop(m)) {
        std::cout << "Got an Event: " << m->conn_id << " - " << m->event << std::endl;

        if (m->event == ring::net::CONNECTED) {
            ring::net::manager.conn_mutex.lock();
            auto find = ring::net::manager.connections.find(m->conn_id);
            if (find != ring::net::manager.connections.end()) conns.emplace(m->conn_id, find->second);
            ring::net::manager.conn_mutex.unlock();
        }
        if (m->event == ring::net::DISCONNECTED) {
            conns.erase(m->conn_id);
        }
        if (m->event == ring::net::TIMEOUT) {
            conns.erase(m->conn_id);
        }
    }
        nlohmann::json *j;
        for(auto &c : conns) {
            if(auto con = c.second.lock()) {
                if(con->game_messages.pop(j)) {
                    std::cout << "Message from " << con->conn_id << std::endl;
                    std::cout << j->dump(4) << std::endl;
                    con->sendLine("Echoing: " + j->dump(4));
                    delete j;
                };
            }
        }
    timer.async_wait([&](auto ec) {check_status(ec, timer); });
}

int main(int argc, char **argv) {

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
        copyover_recover();
    }
    boost::asio::steady_timer start_timer(ring::net::manager.executor, boost::asio::chrono::milliseconds(300));
    start_timer.async_wait([&](auto ec) {check_status(ec, start_timer); });
    ring::net::manager.run();

    if(copyover) {
        std::cout << "Attempting copyover!" << std::endl;
        execl("./ringnet_test", "ringnet_test", nullptr);
    } else {
        std::cout << "okay we done!" << std::endl;
    }
}