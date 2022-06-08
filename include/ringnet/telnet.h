//
// Created by volund on 11/12/21.
//

#ifndef RINGMUD_TELNET_H
#define RINGMUD_TELNET_H

#include "connection.h"

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

    class MudTelnetConnection;
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

    opt_type<TelnetMessage> parse_message(boost::asio::streambuf &buf);

    struct TelnetOptionPerspective {
        bool enabled = false, negotiating = false, answered = false;
    };

    class TelnetOption {
    public:
        TelnetOption(MudTelnetConnection *prot, uint8_t code);
        uint8_t opCode() const;
        bool startWill() const, startDo() const, supportLocal() const, supportRemote() const;
        void enableLocal(), enableRemote(), disableLocal(), disableRemote();
        void receiveNegotiate(uint8_t command);
        void subNegotiate(const TelnetMessage &msg);
        void rejectLocalHandshake(), acceptLocalHandshake(), rejectRemoteHandshake(), acceptRemoteHandshake();
        TelnetOptionPerspective local, remote;
        uint8_t code;
        void load(nlohmann::json& j);
        nlohmann::json serialize() const;
        std::string mtts_last;
    protected:
        MudTelnetConnection *conn;
        int mtts_count = 0;
        void subMTTS(const TelnetMessage &msg);
        void subMTTS_0(const std::string& mtts);
        void subMTTS_1(const std::string& mtts);
        void subMTTS_2(const std::string mtts);
    };


class MudTelnetConnection : public ring::net::MudConnection {
    public:
        MudTelnetConnection(std::string &conn_id, boost::asio::io_context &con);
        MudTelnetConnection(std::string &conn_id, boost::asio::io_context &con, nlohmann::json &j);
        virtual void start() override;
        virtual void sendBytes(const std::vector<uint8_t>& data) = 0;
        virtual void sendJson(const nlohmann::json &j) override;
        virtual void sendPrompt(const std::string &txt) override;
        virtual void sendLine(const std::string &txt) override;
        virtual void sendText(const std::string &txt, net::TextType mode) override;
        virtual void sendMSSP(const std::vector<std::tuple<std::string, std::string>> &data) override;
        virtual nlohmann::json serialize() override;
        virtual void loadJson(nlohmann::json &j) override;
        void sendSub(const uint8_t op, const std::vector<uint8_t>& data);
        void sendNegotiate(uint8_t command, const uint8_t option);
        virtual void resume();
        virtual void onClose();
    protected:
        void handleMessage(const TelnetMessage &msg);
        void handleAppData(const TelnetMessage &msg);
        void handleCommand(const TelnetMessage &msg);
        void handleNegotiate(const TelnetMessage &msg);
        void handleSubnegotiate(const TelnetMessage &msg);
        void onDataReceived();
        void onConnect();
        void ready();
        std::mutex out_mutex, buf_mutex;
        std::string app_data;
        std::unordered_map<uint8_t, TelnetOption> handlers;
        boost::asio::steady_timer start_timer;
        boost::asio::streambuf in_buffer, out_buffer, ex_buffer;
        nlohmann::json serializeHandlers();
    };

    class TcpMudTelnetConnection : public MudTelnetConnection {
    public:
        TcpMudTelnetConnection(std::string &conn_id, boost::asio::io_context &con);
        TcpMudTelnetConnection(std::string &conn_id, boost::asio::io_context &con, nlohmann::json &j, boost::asio::ip::tcp prot, int socket);
        boost::asio::ip::tcp::socket _socket;
        virtual nlohmann::json serialize() override;
        virtual void start() override;
        virtual void sendBytes(const std::vector<uint8_t> &data) override;
    protected:
        bool isWriting = false;
        void read();
        void write();
        void do_read(boost::system::error_code ec, std::size_t trans);
        void do_write(boost::system::error_code ec, std::size_t trans);
        void real_write();
    };

}

#endif //RINGMUD_TELNET_H
