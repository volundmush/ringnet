//
// Created by volund on 6/7/22.
//

#ifndef RINGNET_CONNECTION_H
#define RINGNET_CONNECTION_H

#include "sysdeps.h"

#include "boost/asio.hpp"
#include "boost/lockfree/spsc_queue.hpp"
#include "nlohmann/json.hpp"

namespace ring::net {

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

    enum MsgType : uint8_t {
        Command = 0,
        GMCP = 1,
        MSSP = 2
    };

    struct client_details {
        ClientType clientType = TcpTelnet;
        ColorType colorType = NoColor;
        std::string clientName = "UNKNOWN", clientVersion = "UNKNOWN";
        std::string hostIp = "UNKNOWN", hostName = "UNKNOWN";
        int width = 78, height = 24;
        bool utf8 = false, screen_reader = false, proxy = false, osc_color_palette = false;
        bool vt100 = false, mouse_tracking = false, naws = false, msdp = false, gmcp = false;
        bool mccp2 = false, mccp2_active = false, mccp3 = false, mccp3_active = false, telopt_eor = false;
        bool mtts = false, ttype = false, mnes = false, suppress_ga = false, mslp = false;
        bool force_endline = false, linemode = false, mssp = false, mxp = false, mxp_active = false;

        bool isSecure() const;
        bool supportsOOB() const;
        nlohmann::json serialize();
        void load(nlohmann::json &j);
    };

    enum ConnectionEvent {
        CONNECTED = 0,
        DISCONNECTED = 1,
        TIMEOUT = 2
    };

    struct ConnectionMsg {
        std::string conn_id;
        ConnectionEvent event;
    };

    struct GameMsg {
        std::string command;
        std::string gmcp;
        bool mssp;
    };

    class MudConnection {
    public:
        MudConnection(std::string &conn_id, boost::asio::io_context &con);
        MudConnection(std::string &conn_id, boost::asio::io_context &con, nlohmann::json &j);
        virtual void start() = 0;
        virtual void onClose() = 0;
        virtual void sendPrompt(const std::string &txt);
        virtual void sendText(const std::string &txt, TextType mode) = 0;
        virtual void sendLine(const std::string &txt) = 0;
        virtual void sendJson(const nlohmann::json &j) = 0;
        virtual void sendMSSP(const std::vector<std::tuple<std::string, std::string>> &data) = 0;
        virtual nlohmann::json serialize() = 0;
        virtual void resume() = 0;
        std::string conn_id;
        client_details details;
        bool active = true;
        boost::lockfree::spsc_queue<GameMsg> game_messages;
    protected:
        boost::asio::io_context::strand conn_strand;
        virtual void loadJson(nlohmann::json &j);
    };

}



#endif //RINGNET_CONNECTION_H
