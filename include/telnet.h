//
// Created by volund on 11/12/21.
//

#ifndef RINGMUD_TELNET_H
#define RINGMUD_TELNET_H

#include <cstdint>
#include <vector>
#include <string>
#include <unordered_map>
#include <map>
#include <optional>
#include "asio.hpp"
#include "entt/entt.hpp"

namespace ring::net {
    struct connection_details;
}

namespace ring::telnet {

    namespace codes {
        extern const uint8_t NUL;
        extern const uint8_t BEL;
        extern const uint8_t CR;
        extern const uint8_t LF;
        extern const uint8_t SGA;
        extern const uint8_t TELOPT_EOR;
        extern const uint8_t NAWS;
        extern const uint8_t LINEMODE;
        extern const uint8_t EOR;
        extern const uint8_t SE;
        extern const uint8_t NOP;
        extern const uint8_t GA;
        extern const uint8_t SB;
        extern const uint8_t WILL;
        extern const uint8_t WONT;
        extern const uint8_t DO;
        extern const uint8_t DONT;
        extern const uint8_t IAC;

        extern const uint8_t MNES;
        extern const uint8_t MXP;
        extern const uint8_t MSSP;
        extern const uint8_t MCCP2;
        extern const uint8_t MCCP3;

        extern const uint8_t GMCP;
        extern const uint8_t MSDP;
        extern const uint8_t MTTS;
    }

    class TelnetProtocol;
    class TelnetOption;

    enum TelnetMsgType : uint8_t {
        AppData = 0, // random telnet bytes
        Command = 1, // an IAC <something>
        Negotiation = 2, // an IAC WILL/WONT/DO/DONT
        Subnegotiation = 3 // an IAC SB <code> <data> IAC SE
    };

    struct TelnetMessage {
        explicit TelnetMessage(TelnetMsgType m_type);
        TelnetMsgType msg_type;
        std::vector<uint8_t> data;
        uint8_t codes[2] = {0, 0};
    };

    std::optional<TelnetMessage> parse_message(asio::streambuf &buf);

    struct TelnetOptionPerspective {
        bool enabled = false, negotiating = false, answered = false;
    };

    class TelnetOption {
    public:
        explicit TelnetOption(TelnetProtocol &prot, uint8_t code);
        uint8_t opCode() const;
        bool startWill() const, startDo() const, supportLocal() const, supportRemote() const;
        void enableLocal(), enableRemote(), disableLocal(), disableRemote();
        void receiveNegotiate(uint8_t command);
        void subNegotiate(const TelnetMessage &msg);
        void rejectLocalHandshake(), acceptLocalHandshake(), rejectRemoteHandshake(), acceptRemoteHandshake();
        TelnetOptionPerspective local, remote;
        uint8_t code;
    protected:
        TelnetProtocol &protocol;
        bool ready = false;
    };

    class TelnetProtocol {
    public:
        explicit TelnetProtocol(net::connection_details &det);
        void handleMessage(const TelnetMessage &msg);
        void handleAppData(const TelnetMessage &msg);
        void handleCommand(const TelnetMessage &msg);
        void handleNegotiate(const TelnetMessage &msg);
        void handleSubnegotiate(const TelnetMessage &msg);
        void sendNegotiate(uint8_t command, const uint8_t option);
        void sendSub(const uint8_t op, const std::vector<uint8_t>& data);
        void sendBytes(const std::vector<uint8_t>& data);
        void sendText(const std::string& txt);
        void sendLine(const std::string& txt);
        void sendPrompt(const std::string& prompt);
        void sendMSSP(std::map<std::string, std::string>& data);
        void start(), onConnect();
        net::connection_details &conn;
    private:
        uint32_t overflow_counter = 0;
        std::string app_data;
        std::unordered_map<uint8_t, TelnetOption> handlers;
        asio::steady_timer start_timer;
    };


}

template<>
struct entt::component_traits<ring::telnet::TelnetProtocol>: basic_component_traits {
    using in_place_delete = std::true_type;
};

#endif //RINGMUD_TELNET_H
