//
// Created by volund on 11/12/21.
//

#include <iostream>
#include <thread>
#include "ringnet/net.h"
#include "ringnet/telnet.h"

namespace ring::net {

    asio::io_context executor;

    std::function<void(int conn_id)> on_ready_cb, on_close_cb, on_receive_cb;

    client_details::client_details(ClientType ctype) {
        clientType = ctype;
    }

    bool client_details::isSecure() const {
        switch(clientType) {
            case TlsTelnet:
            case WebSocket:
                return true;
            default:
                return false;
        }
    }

    bool client_details::supportsOOB() const {
        if(clientType == WebSocket) return true;
        return gmcp || msdp;

    }

    void socket_buffers::write(const std::vector<uint8_t> &data) {
        out_mutex.lock();
        auto prep = out_buffer.prepare(data.size());
        memcpy(prep.data(), data.data(), data.size());
        out_buffer.commit(data.size());
        out_mutex.unlock();
    }

    connection_details::connection_details(int con, ClientType ctype) : details(ctype) {
        conn_id = con;
        switch(ctype) {
            case TcpTelnet:
            case TlsTelnet:
                buffers = new socket_buffers;
                telnetProtocol = new telnet::TelnetProtocol(*this);
                break;
            default:
                break;
        }
    }

    connection_details::~connection_details() {
        delete buffers;
        delete plainSocket;
        delete telnetProtocol;
    }

    void connection_details::onReady() {
        active = true;
        on_ready_cb(conn_id);
    }

    void connection_details::onClose() {
        if(active) on_close_cb(conn_id);
        active = false;
        delete plainSocket;
    }

    void connection_details::receiveJson(const nlohmann::json &json) {
        nlohmann::json j = {
                {"msg_type", 1},
                {"data", json}
        };
        queueJson(j);
    }

    void connection_details::receiveText(const std::string &txt, TextType mode) {
        nlohmann::json j = {
                {"msg_type", 0},
                {"data", {
                        {"text", txt},
                        {"mode", mode},
                    }
                }
        };
        queueJson(j);
    }

    void connection_details::queueJson(const nlohmann::json &json) {
        in_queue_mutex.lock();
        queue_in.push(json);
        in_queue_mutex.unlock();
        on_receive_cb(conn_id);
    }

    plain_socket::plain_socket() : socket(executor) {

    }



    void plain_socket::send() {
        if(!isWriting) {
            auto &out_buffer = conn->buffers->out_buffer;
            if(out_buffer.size()) {
                isWriting = true;
                conn->buffers->out_mutex.lock();
                auto handler = [&](std::error_code ec, std::size_t trans) {
                    out_buffer.consume(trans);
                    conn->buffers->out_mutex.unlock();
                    isWriting = false;
                    if(out_buffer.size())
                        send();
                };
                auto b = asio::buffer(out_buffer.data());
                socket.async_write_some(b, handler);
            }
        }
    }

    void plain_socket::receive() {
        if(!isReading) {
            isReading = true;
            auto &in_buffer = conn->buffers->in_buffer;
            auto prep = in_buffer.prepare(1024);
            auto handler = [&](std::error_code ec, std::size_t trans) {
                if(!ec) {
                    in_buffer.commit(trans);
                    onDataReceived();
                    // re-arm the socket and call receive again.
                    isReading = false;
                    receive();
                } else {
                    if(ec == asio::error::eof) {
                        conn->onClose();
                    }
                }
            };
            socket.async_read_some(asio::buffer(prep), handler);
        }
    }

    void plain_socket::onDataReceived() {
        auto &in_buffer = conn->buffers->in_buffer;
        auto &details = conn->details;

        if(details.clientType == TcpTelnet || details.clientType == TlsTelnet) {
            auto msg = ring::telnet::parse_message(in_buffer);
            if(msg.has_value()) {
                conn->telnetProtocol->handleMessage(msg.value());
            }
        }
    }

    plain_telnet_listen::plain_telnet_listen(asio::ip::tcp::endpoint endp, ListenManager &man)
    : manager(man), acceptor(executor, endp) {}

    void plain_telnet_listen::listen() {
        if(!isListening) {
            isListening = true;
            queued_socket = new plain_socket;
            auto handler = [&](std::error_code ec) {
                if(!ec) {
                    auto new_conn = new connection_details(manager.next_id++, TcpTelnet);
                    queued_socket->conn = new_conn;
                    new_conn->plainSocket = queued_socket;
                    queued_socket = nullptr;
                    manager.conn_mutex.lock();
                    manager.connections.emplace(new_conn->conn_id, new_conn);
                    manager.conn_mutex.unlock();
                    new_conn->plainSocket->receive();
                    new_conn->telnetProtocol->start();
                } else {
                    // Oops, something bad happened. handle it!
                }
                isListening = false;
                listen();
            };
            acceptor.async_accept(queued_socket->socket, handler);
        }
    }

    ListenManager::ListenManager() {};

    bool ListenManager::readyTLS() {return false;};

    asio::ip::address ListenManager::parse_addr(const std::string &ip) {
        std::error_code ec;
        auto ip_address = asio::ip::address::from_string(ip, ec);

        if(ec) {
            std::cerr << "Failed to parse IP Address: " << ip << " Error code: " << ec.value() << " - " << ec.message() << std::endl;
            exit(1);
        }
        return ip_address;
    }

    asio::ip::tcp::endpoint ListenManager::create_endpoint(const std::string &ip, uint16_t port) {
        if(ports.contains(port)) {
            std::cerr << "Port is already in use: " << port << std::endl;
            exit(1);
        }
        return asio::ip::tcp::endpoint(parse_addr(ip), port);
    }

    bool ListenManager::listenPlainTelnet(const std::string& ip, uint16_t port) {
        auto endp = create_endpoint(ip, port);
        auto listener = new plain_telnet_listen(endp, *this);
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

    void run() {
        if(!on_ready_cb) {
            std::cerr << "Error! on_ready_cb not set." << std::endl;
            exit(1);
        }
        if(!on_close_cb) {
            std::cerr << "Error! on_close_cb not set." << std::endl;
            exit(1);
        }
        if(!on_receive_cb) {
            std::cerr << "Error! on_receive_cb not set." << std::endl;
        }
        // quick and dirty
        for(int i = 0; i < std::thread::hardware_concurrency() - 1; i++) {
            std::thread t([](){executor.run();});
            manager.threads.push_back(std::move(t));
        }
        executor.run();
    }

    void ListenManager::closeConn(int conn_id) {
        conn_mutex.lock();
        if(!connections.contains(conn_id)) {
            conn_mutex.unlock();
            return;
        }

        connections.erase(conn_id);
        conn_mutex.unlock();
    }

}