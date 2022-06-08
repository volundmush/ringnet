//
// Created by volund on 11/12/21.
//


#include "ringnet/net.h"
#include <random>

namespace ring::net {


    plain_telnet_listen::plain_telnet_listen(ListenManager &man, boost::asio::ip::tcp::endpoint endp)
    : manager(man), acceptor(man.executor, endp), listen_strand(man.executor) {}

    plain_telnet_listen::plain_telnet_listen(ListenManager &man, boost::asio::ip::tcp prot, int socket)
    : acceptor(man.executor, prot, socket), manager(man), listen_strand(man.executor) {}

    void plain_telnet_listen::do_listen() {
        manager.id_mutex.lock();
        auto new_id = generate_id("telnet", 10, manager.conn_ids);
        manager.id_mutex.unlock();

        queued_connection = new telnet::TcpMudTelnetConnection(new_id, manager.executor);

        acceptor.async_accept(queued_connection->_socket, [this](auto ec) { do_accept(ec); });
    }

    void plain_telnet_listen::do_accept(std::error_code ec) {
        if(ec) {
            // Oops, something bad happened. handle it!
        }
        manager.conn_mutex.lock();
        manager.connections.emplace(queued_connection->conn_id, queued_connection);
        manager.conn_mutex.unlock();
        queued_connection->start();
        do_listen();
    }

    void plain_telnet_listen::listen() {

        listen_strand.post([this] { do_listen(); });
    }

    ListenManager::ListenManager() : events(128) {};

    bool ListenManager::readyTLS() {return false;};

    boost::asio::ip::address ListenManager::parse_addr(const std::string &ip) {
        std::error_code ec;
        auto ip_address = boost::asio::ip::address::from_string(ip);

        if(ec) {
            std::cerr << "Failed to parse IP Address: " << ip << " Error code: " << ec.value() << " - " << ec.message() << std::endl;
            exit(1);
        }
        return ip_address;
    }

    boost::asio::ip::tcp::endpoint ListenManager::create_endpoint(const std::string &ip, uint16_t port) {
        if(ports.count(port)) {
            std::cerr << "Port is already in use: " << port << std::endl;
            exit(1);
        }
        return boost::asio::ip::tcp::endpoint(parse_addr(ip), port);
    }

    bool ListenManager::listenPlainTelnet(const std::string& ip, uint16_t port) {
        auto endp = create_endpoint(ip, port);
        auto listener = new plain_telnet_listen(*this, endp);
        plain_telnet_listeners.emplace(port, listener);
        listener->listen();
        return true;
    }

    bool ListenManager::listenTLSTelnet(const std::string& ip, uint16_t port) {
        return false;
    }

    bool ListenManager::listenWebSocket(const std::string& ip, uint16_t port) {
        return false;
    }

    ListenManager manager;

    void ListenManager::run(int threads) {

        int thread_count = threads;
        if(thread_count < 1)
            thread_count = std::thread::hardware_concurrency();

        // quick and dirty
        for(int i = 0; i < thread_count - 1; i++) {
            manager.threads.emplace_back([this](){executor.run();});
        }

        executor.run();

        for(auto &t : manager.threads) {
            t.join();
        }
        manager.threads.clear();

    }

    nlohmann::json ListenManager::copyover() {
        executor.stop();
        auto j = serialize();
        running = false;
        return j;
    }

    void ListenManager::closeConn(std::string &conn_id) {
        conn_mutex.lock();
        auto f = connections.find(conn_id);
        if(f != connections.end()) {
            f->second->onClose();
            connections.erase(conn_id);
        }
        conn_mutex.unlock();
    }

    nlohmann::json ListenManager::serialize() {
        nlohmann::json j;
        j["plainTelnetListeners"] = serializePlainTelnetListeners();
        j["connections"] = serializeConnections();
        return j;
    }

    nlohmann::json ListenManager::serializePlainTelnetListeners() {
        auto j = nlohmann::json::array();
        for(const auto& t : plain_telnet_listeners) {
            nlohmann::json j2 = {
                    {"socket", t.second->acceptor.native_handle()},
                    {"port", t.first}
            };
            if(t.second->acceptor.local_endpoint().protocol() == boost::asio::ip::tcp::v4()) {
                j2["protocol_type"] = 4;
            } else {
                j2["protocol_type"] = 6;
            }

            j.push_back(j2);
        }
        return j;
    }

    nlohmann::json ListenManager::serializeConnections() {
        auto j = nlohmann::json::array();
        for(const auto& t : connections) {
            j.push_back(t.second->serialize());
        }
        return j;
    }

    void ListenManager::copyoverRecover(nlohmann::json &json) {

        if(json.contains("plainTelnetListeners")) {
            loadPlainTelnetListeners(json.at("plainTelnetListeners"));
        }

        if(json.contains("connections")) {
            loadConnections(json.at("connections"));
        }

        for(auto &l : plain_telnet_listeners) {
            l.second->listen();
        }

        for(auto &c : connections) {
            c.second->resume();
        }
    }

    void ListenManager::loadPlainTelnetListeners(nlohmann::json &j) {
        for(const auto &j2 : j) {
            int socket = j2["socket"];
            int prot = j2["protocol_type"];
            auto p = new plain_telnet_listen(*this, prot==4 ? boost::asio::ip::tcp::v4() : boost::asio::ip::tcp::v6(), socket);
            int port = j2["port"];
            ports.insert(port);
            plain_telnet_listeners.emplace(port, p);
        }
    }

    void ListenManager::loadConnections(nlohmann::json &j) {
        for(auto &j2 : j) {
            std::string conn_id = j2["conn_id"];
            net::ClientType c = j2["details"]["clientType"];
            switch(c) {
                case ring::net::TcpTelnet:
                    loadPlainTelnet(j2);
                    break;
                case ring::net::TlsTelnet:
                    loadTlsTelnet(j2);
                    break;
                default:
                    break;
            }
        }
    }

    void ListenManager::loadPlainTelnet(nlohmann::json &j) {
        std::string conn_id = j["conn_id"];
        int prot = j["protocol"];
        boost::asio::ip::tcp p = prot ? boost::asio::ip::tcp::v4() : boost::asio::ip::tcp::v6();
        int socket = j["socket"];
        auto c = new telnet::TcpMudTelnetConnection(conn_id, executor, j, p, socket);
    }

    void ListenManager::loadTlsTelnet(nlohmann::json &j) {

    }


    std::string random_string(std::size_t length)
    {
        const std::string characters = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

        std::random_device random_device;
        std::mt19937 generator(random_device());
        std::uniform_int_distribution<> distribution(0, characters.size() - 1);

        std::string random_string;

        for (std::size_t i = 0; i < length; ++i)
        {
            random_string += characters[distribution(generator)];
        }

        return random_string;
    }

    std::string generate_id(const std::string &prf, std::size_t length, std::set<std::string> &existing) {
        auto generated = prf + "_" + random_string(length);
        while(existing.count(generated)) {
            generated = prf + "_" + random_string(length);
        }
        existing.insert(generated);
        return generated;
    }

}