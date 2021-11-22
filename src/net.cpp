//
// Created by volund on 11/12/21.
//


#include "ringnet/net.h"
#include "ringnet/telnet.h"
#include "base64_default_rfc4648.hpp"

namespace ring::net {

    asio::io_context *executor = new asio::io_context;

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

    nlohmann::json client_details::serialize() {
        nlohmann::json j = {
                {"clientType", clientType},
                {"colorType", colorType},
                {"clientName", clientName},
                {"clientVersion", clientVersion},
                {"hostIp", hostIp},
                {"hostName", hostName},
                {"width", width},
                {"height", height},
                {"utf8", utf8},
                {"screen_reader", screen_reader},
                {"proxy", proxy},
                {"osc_color_palette", osc_color_palette},
                {"vt100", vt100},
                {"mouse_tracking", mouse_tracking},
                {"naws", naws},
                {"msdp", msdp},
                {"gmcp", gmcp},
                {"mccp2", mccp2},
                {"mccp2_active", mccp2_active},
                {"mccp3", mccp3},
                {"mccp3_active", mccp3_active},
                {"mtts", mtts},
                {"ttype", ttype},
                {"mnes", mnes},
                {"suppress_ga", suppress_ga},
                {"force_endline", force_endline},
                {"linemode", linemode},
                {"mssp", mssp},
                {"mxp", mxp},
                {"mxp_active", mxp_active}
        };
        return j;
    }

    void client_details::load(nlohmann::json &j) {
        clientType = j["clientType"];
        colorType = j["colorType"];
        clientName = j["clientName"];
        clientVersion = j["clientVersion"];
        hostIp = j["hostIp"];
        hostName = j["hostName"];
        width = j["width"];
        height = j["height"];
        utf8 = j["utf8"];
        screen_reader = j["screen_reader"];
        proxy = j["proxy"];
        osc_color_palette = j["osc_color_palette"];
        vt100 = j["vt100"];
        mouse_tracking = j["mouse_tracking"];
        naws = j["naws"];
        msdp = j["msdp"];
        gmcp = j["gmcp"];
        mccp2 = j["mccp2"];
        mccp2_active = j["mccp2_active"];
        mccp3 = j["mccp3"];
        mccp3_active = j["mccp3_active"];
        mtts = j["mtts"];
        ttype = j["ttype"];
        mnes = j["mnes"];
        suppress_ga = j["suppress_ga"];
        force_endline = j["force_endline"];
        linemode = j["linemode"];
        mssp = j["mssp"];
        mxp = j["mxp"];
        mxp_active = j["mxp_active"];
    }

    socket_buffers::socket_buffers() {

    }

    socket_buffers::socket_buffers(nlohmann::json &j) {
        using base64 = cppcodec::base64_rfc4648;
        std::string in_data = j["in_buffer"], out_data = j["out_buffer"];
        if(!in_data.empty()) {

        }
    }

    void socket_buffers::write(const std::vector<uint8_t> &data) {
        out_mutex.lock();
        auto prep = out_buffer.prepare(data.size());
        memcpy(prep.data(), data.data(), data.size());
        out_buffer.commit(data.size());
        out_mutex.unlock();
    }

    nlohmann::json socket_buffers::serialize() const {
        using base64 = cppcodec::base64_rfc4648;
        nlohmann::json j = {
                {"in_buffer", base64::encode((uint8_t*)in_buffer.data().data(), in_buffer.data().size())},
                {"out_buffer", base64::encode((uint8_t*)out_buffer.data().data(), out_buffer.data().size())}
        };
        return j;
    }

    connection_details::connection_details(int con, ClientType ctype) : details(ctype) {
        conn_id = con;
        switch(ctype) {
            case TcpTelnet:
            case TlsTelnet:
                buffers = std::make_shared<socket_buffers>();
                telnetProtocol = std::make_shared<telnet::TelnetProtocol>(*this);
                break;
            default:
                break;
        }
    }

    connection_details::connection_details(const nlohmann::json &j) : details(TcpTelnet) {
        conn_id = j["conn_id"];
        auto jd = j["details"];
        details.load(jd);
        if(details.clientType == TcpTelnet) {
            auto sd = j["plainSocket"];
            int prot_id = sd["protocol"];
            int socket = sd["socket"];
            plainSocket = std::make_shared<plain_socket>(prot_id==4 ? asio::ip::tcp::v4() : asio::ip::tcp::v6(), socket);
            plainSocket->conn = this;
        }
        if(details.clientType == TlsTelnet) {
            // Tls stuff goes here...
        }

        if(details.clientType == TcpTelnet || details.clientType == TlsTelnet) {
            auto jbuf = j["buffers"];
            buffers = std::make_shared<socket_buffers>(jbuf);
            telnetProtocol = std::make_shared<telnet::TelnetProtocol>(*this);
        }

        switch(details.clientType) {
            case TcpTelnet:
            case TlsTelnet:
                buffers = std::make_shared<socket_buffers>();
                break;
            default:
                break;
        }
    }

    void connection_details::onReady() {
        active = true;
        on_ready_cb(conn_id);
    }

    void connection_details::onClose() {
        if(active) on_close_cb(conn_id);
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
        queue_in.push_back(json);
        in_queue_mutex.unlock();
        on_receive_cb(conn_id);
    }

    void connection_details::sendJson(const nlohmann::json &json) {
        if(telnetProtocol) {

        }
    }

    void connection_details::sendText(const std::string &txt, TextType mode) {
        if(telnetProtocol) {
            switch(mode) {
                case Line:
                    telnetProtocol->sendLine(txt);
                    break;
                case Text:
                    telnetProtocol->sendText(txt);
                    break;
                case Prompt:
                    telnetProtocol->sendPrompt(txt);
                    break;
            }
        }
    }

    void connection_details::sendMSSP(const std::vector<std::pair<std::string, std::string>> &pairs) {

    }

    void connection_details::receiveMSSP() {
        nlohmann::json j = {
                {"msg_type", 3},
                {"data", nullptr}
        };
        queueJson(j);
    }

    nlohmann::json connection_details::serialize() {
        nlohmann::json j;
        j["conn_id"] = conn_id;
        j["details"] = details.serialize();
        if(telnetProtocol) {
            j["telnetProtocol"] = telnetProtocol->serialize();
        }
        if(buffers) {
            j["buffers"] = buffers->serialize();
        }
        if(!queue_in.empty()) {
            j["queue"] = queue_in;
        }
        if(plainSocket) {
            j["plainSocket"] = plainSocket->serialize();
        }
        return j;
    }

    void connection_details::resume() {
        if(plainSocket) {
            if(buffers->out_buffer.size()) {
                plainSocket->send();
            }
            plainSocket->receive();
        }
    }

    plain_socket::plain_socket() : socket(*executor) {

    }

    plain_socket::plain_socket(asio::ip::tcp prot, int socket) : socket(*executor, prot, socket) {

    }

    nlohmann::json plain_socket::serialize() {
        nlohmann::json j;
        j["socket"] = socket.native_handle();
        j["protocol"] = socket.local_endpoint().protocol() == asio::ip::tcp::v4() ? 4 : 6;
        return j;
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
        auto &details = conn->details;

        if(details.clientType == TcpTelnet || details.clientType == TlsTelnet) {
            while(true) {
                auto msg = ring::telnet::parse_message(conn->buffers->in_buffer);
                if(msg.has_value()) {
                    auto msg_val = msg.value();
                    conn->telnetProtocol->handleMessage(msg.value());
                }
                else
                    break;
            }
        }
    }

    plain_telnet_listen::plain_telnet_listen(asio::ip::tcp::endpoint endp, ListenManager &man)
    : manager(man), acceptor(*executor, endp) {}

    plain_telnet_listen::plain_telnet_listen(ListenManager &man, asio::ip::tcp prot, int socket)
    : acceptor(*executor, prot, socket), manager(man) {}

    void plain_telnet_listen::listen() {
        if(!isListening) {
            isListening = true;
            queued_socket = std::make_shared<plain_socket>();
            auto handler = [&](std::error_code ec) {
                if(!ec) {
                    auto new_conn = new connection_details(manager.next_id++, TcpTelnet);
                    queued_socket->conn = new_conn;
                    new_conn->plainSocket = queued_socket;
                    queued_socket.reset();
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

    ListenManager::ListenManager() = default;

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
        if(ports.count(port)) {
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

    void ListenManager::run() {
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
        for(int i = 0; i < std::max(std::thread::hardware_concurrency(), 2U) - 1; i++) {
            std::thread t([](){executor->run();});
            manager.threads.push_back(std::move(t));
        }

        executor->run();

        for(auto &t : manager.threads) {
            t.join();
        }
        manager.threads.clear();
    }

    nlohmann::json ListenManager::copyover() {
        executor->stop();
        auto j = serialize();
        delete executor;
        return j;
    }

    void ListenManager::closeConn(int conn_id) {
        conn_mutex.lock();
        connections.erase(conn_id);
        conn_mutex.unlock();
    }

    nlohmann::json ListenManager::serialize() {
        nlohmann::json j;
        int val = next_id;
        j["next_id"] = val;
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
            if(t.second->acceptor.local_endpoint().protocol() == asio::ip::tcp::v4()) {
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
        next_id = json["next_id"];
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
            auto p = new plain_telnet_listen(*this, prot==4 ? asio::ip::tcp::v4() : asio::ip::tcp::v6(), socket);
            int port = j2["port"];
            ports.insert(port);
            plain_telnet_listeners.emplace(port, p);
        }
    }

    void ListenManager::loadConnections(nlohmann::json &j) {
        for(const auto &j2 : j) {
            int conn_id = j2["conn_id"];
            connections.emplace(conn_id, new connection_details(j2));
        }
    }

}