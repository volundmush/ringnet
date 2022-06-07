//
// Created by volund on 11/12/21.
//

#ifndef RINGMUD_NET_H
#define RINGMUD_NET_H

#include "sysdeps.h"

#include "boost/asio.hpp"
#include "nlohmann/json.hpp"
#include "telnet.h"


namespace ring::net {

    extern boost::asio::io_context *executor;

    extern std::function<void(uint64_t conn_id)> on_ready_cb, on_close_cb, on_receive_cb;

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
        bool mtts = false, ttype = false, mnes = false, suppress_ga = false, mslp = false;
        bool force_endline = false, linemode = false, mssp = false, mxp = false, mxp_active = false;

        explicit client_details(ClientType ctype);
        bool isSecure() const;
        bool supportsOOB() const;
        nlohmann::json serialize();
        void load(nlohmann::json &j);
    };

    struct socket_buffers {
        boost::asio::streambuf in_buffer, out_buffer;
        std::mutex out_mutex;
        void write(const std::vector<uint8_t> &data);
        nlohmann::json serialize() const;
        socket_buffers();
        socket_buffers(nlohmann::json &j);
    };

    struct plain_socket {
        explicit plain_socket();
        plain_socket(boost::asio::ip::tcp prot, int socket);
        boost::asio::ip::tcp::socket socket;
        bool isWriting = false;
        bool isReading = false;
        connection_details *conn = nullptr;
        void sendBytes(const std::vector<uint8_t> &data);
        void send();
        void receive();
        void onDataReceived();
        nlohmann::json serialize();
    };

    struct connection_details {
        connection_details(uint64_t con, ClientType ctype);
        connection_details(const nlohmann::json &j);
        ~connection_details();
        uint64_t conn_id;
        client_details details;
        std::shared_ptr<plain_socket> plainSocket;
        std::shared_ptr<socket_buffers> buffers;
        std::shared_ptr<telnet::TelnetProtocol> telnetProtocol;
        std::mutex in_queue_mutex;
        std::list<nlohmann::json> queue_in;
        bool active = true;
        void onClose();
        void onReady();
        void setup();
        void sendText(const std::string &txt, TextType mode = Text);
        void sendJson(const nlohmann::json& json);
        void sendMSSP(const std::vector<std::pair<std::string, std::string>>& pairs);
        void receiveText(const std::string &txt, TextType mode = Text);
        void receiveJson(const nlohmann::json& json);
        void queueJson(const nlohmann::json& json);
        void receiveMSSP();
        nlohmann::json serialize();
        void resume();

    };

    struct plain_telnet_listen {
        plain_telnet_listen(boost::asio::ip::tcp::endpoint endp, ListenManager &man);
        plain_telnet_listen(ListenManager &man, boost::asio::ip::tcp prot, int socket);
        boost::asio::ip::tcp::acceptor acceptor;
        std::shared_ptr<plain_socket> queued_socket;
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
        std::atomic<uint64_t> next_id = 0;
        std::unordered_map<uint64_t, std::shared_ptr<connection_details>> connections;
        std::mutex conn_mutex;
        void closeConn(uint64_t conn_id);
        void run(int threads = 0);
        nlohmann::json copyover();
        std::vector<std::thread> threads;
        void copyoverRecover(nlohmann::json &json);
        nlohmann::json serialize();
        bool running = true;
        std::unordered_map<uint16_t, std::unique_ptr<plain_telnet_listen>> plain_telnet_listeners;
    protected:

        std::unordered_set<uint16_t> ports;
        boost::asio::ip::address parse_addr(const std::string& ip);
        boost::asio::ip::tcp::endpoint create_endpoint(const std::string& ip, uint16_t port);
        nlohmann::json serializePlainTelnetListeners();
        nlohmann::json serializeConnections();
        void loadPlainTelnetListeners(nlohmann::json &j);
        void loadConnections(nlohmann::json &j);

    };

    extern ListenManager manager;

}



#endif //RINGMUD_NET_H
