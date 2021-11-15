//
// Created by volund on 11/12/21.
//

#ifndef RINGMUD_NET_H
#define RINGMUD_NET_H

#include <mutex>
#include <memory>
#include <queue>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <atomic>
#include "asio.hpp"
#include "nlohmann/json.hpp"
#include "telnet.h"


namespace ring::net {

    extern asio::io_context executor;

    void run();

    extern std::function<void(int conn_id)> on_ready_cb, on_close_cb, on_receive_cb;


    enum ClientType : uint8_t {
        TcpTelnet = 0,
        TlsTelnet = 1,
        WebSocket = 2
    };

    enum ColorType : uint8_t {
        NoColor = 0,
        StandardColor = 1,
        XtermColor = 2,
        TrueColor = 3
    };

    enum TextType : uint8_t {
        Text = 0,
        Line = 1,
        Prompt = 2
    };

    struct TextMessage {
        TextType textType;
        std::string msg;
    };

    struct connection_details;
    class ListenManager;

    struct client_details {
        ClientType clientType = TcpTelnet;
        ColorType colorType = NoColor;
        std::string clientName = "UNKNOWN", clientVersion = "UNKNOWN";
        std::string hostIp = "UNKNOWN", hostName = "UNKNOWN";
        int width = 78, height = 24;
        bool utf8 = false, screen_reader = false, proxy = false, osc_color_palette = false;
        bool vt100 = false, mouse_tracking = false, naws = false, msdp = false, gmcp = false;
        bool mccp2 = false, mccp2_active = false, mccp3 = false, mccp3_active = false;
        bool mtts = false, ttype = false, mnes = false, suppress_ga = false;
        bool force_endline = false, linemode = false, mssp = false, mxp = false, mxp_active = false;

        explicit client_details(ClientType ctype);
        bool isSecure() const;
        bool supportsOOB() const;
    };

    struct socket_buffers {
        asio::streambuf in_buffer, out_buffer;
        std::mutex out_mutex;
        void write(const std::vector<uint8_t> &data);
    };

    struct plain_socket {
        explicit plain_socket();
        asio::ip::tcp::socket socket;
        bool isWriting = false;
        bool isReading = false;
        connection_details *conn = nullptr;

        void send();
        void receive();
        void onDataReceived();
    };

    struct connection_details {
        connection_details(int con, ClientType ctype);
        ~connection_details();
        int conn_id;
        client_details details;
        plain_socket *plainSocket;
        socket_buffers *buffers = nullptr;
        telnet::TelnetProtocol *telnetProtocol = nullptr;
        std::mutex in_queue_mutex, out_queue_mutex;
        std::queue<nlohmann::json> queue_in, queue_out;
        bool active = true;
        void onClose();
        void onReady();
        void sendText(const std::string &txt, TextType mode = Text);
        void sendJson(const nlohmann::json& json);
        void sendMSSP(const std::vector<std::pair<std::string, std::string>>& pairs);
        void receiveText(const std::string &txt, TextType mode = Text);
        void receiveJson(const nlohmann::json& json);
        void queueJson(const nlohmann::json& json);

    };

    struct plain_telnet_listen {
        plain_telnet_listen(asio::ip::tcp::endpoint endp, ListenManager &man);
        asio::ip::tcp::acceptor acceptor;
        plain_socket *queued_socket;
        ListenManager &manager;

        bool isListening = false;

        void listen();
    };

    class ListenManager {
    public:
        ListenManager();
        bool readyTLS();
        bool listenPlainTelnet(const std::string& ip, uint16_t port);
        bool listenTLSTelnet(const std::string& ip, uint16_t port);
        bool listenWebSocket(const std::string& ip, uint16_t port);
        std::atomic<int> next_id = 0;
        std::unordered_map<int, std::shared_ptr<connection_details>> connections;
        std::mutex conn_mutex;
        void closeConn(int conn_id);
        std::vector<std::thread> threads;
    protected:
        std::unordered_map<uint16_t, std::unique_ptr<plain_telnet_listen>> plain_telnet_listeners;
        std::unordered_set<uint16_t> ports;
        asio::ip::address parse_addr(const std::string& ip);
        asio::ip::tcp::endpoint create_endpoint(const std::string& ip, uint16_t port);

    };

    extern ListenManager manager;

}



#endif //RINGMUD_NET_H
