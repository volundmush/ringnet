//
// Created by volund on 11/12/21.
//

#ifndef RINGMUD_NET_H
#define RINGMUD_NET_H

#include <set>
#include "sysdeps.h"
#include "boost/asio.hpp"
#include "nlohmann/json.hpp"
#include "telnet.h"


namespace ring::net {

    class ListenManager;

    struct plain_telnet_listen {
        plain_telnet_listen(ListenManager &man, boost::asio::ip::tcp::endpoint endp);
        plain_telnet_listen(ListenManager &man, boost::asio::ip::tcp prot, int socket);
        boost::asio::ip::tcp::acceptor acceptor;
        telnet::TcpMudTelnetConnection* queued_connection;
        ListenManager &manager;
        boost::asio::io_context::strand listen_strand;
        bool isListening = false;

        void listen();
        void do_listen();
        void do_accept(std::error_code ec);
    };


    class ListenManager {
    public:
        ListenManager();
        bool readyTLS();
        bool listenPlainTelnet(const std::string& ip, uint16_t port);
        bool listenTLSTelnet(const std::string& ip, uint16_t port);
        bool listenWebSocket(const std::string& ip, uint16_t port);
        std::set<std::string> conn_ids;
        std::unordered_map<std::string, std::shared_ptr<MudConnection>> connections;
        std::mutex conn_mutex, id_mutex;
        void closeConn(std::string &conn_id);
        void run(int threads = 0);
        nlohmann::json copyover();
        std::vector<std::thread> threads;
        void copyoverRecover(nlohmann::json &json);
        nlohmann::json serialize();
        bool running = true;
        boost::asio::io_context executor;
        boost::lockfree::spsc_queue<ConnectionMsg> events;
        std::unordered_map<uint16_t, std::unique_ptr<plain_telnet_listen>> plain_telnet_listeners;
    protected:

        std::unordered_set<uint16_t> ports;
        boost::asio::ip::address parse_addr(const std::string& ip);
        boost::asio::ip::tcp::endpoint create_endpoint(const std::string& ip, uint16_t port);
        nlohmann::json serializePlainTelnetListeners();
        nlohmann::json serializeConnections();
        void loadPlainTelnetListeners(nlohmann::json &j);
        void loadConnections(nlohmann::json &j);
        void loadPlainTelnet(nlohmann::json &j);
        void loadTlsTelnet(nlohmann::json &j);
    };

    std::string random_string(std::size_t length);
    std::string generate_id(const std::string &prf, std::size_t length, std::set<std::string> &existing);

    extern ListenManager manager;

}



#endif //RINGMUD_NET_H
