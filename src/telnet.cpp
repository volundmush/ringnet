//
// Created by volund on 11/12/21.
//

#include <iostream>
#include <chrono>
#include "ringnet/telnet.h"
#include "ringnet/net.h"

namespace ring::telnet {
    namespace codes {

        const uint8_t NUL = 0, BEL = 7, CR = 13, LF = 10, SGA = 3, TELOPT_EOR = 25, NAWS = 31;
        const uint8_t LINEMODE = 34, EOR = 239, SE = 240, NOP = 241, GA = 249, SB = 250;
        const uint8_t WILL = 251, WONT = 252, DO = 253, DONT = 254, IAC = 255, MNES = 39;
        const uint8_t MXP = 91, MSSP = 70, MCCP2 = 86, MCCP3 = 87, GMCP = 201, MSDP = 69;
        const uint8_t MTTS = 24;
    }

    TelnetMessage::TelnetMessage(TelnetMsgType m_type) {
        msg_type = m_type;
    }

    std::optional<TelnetMessage> parse_message(asio::streambuf &buf) {
        using namespace ring::telnet::codes;
        // return early if nothing to do.
        auto available = buf.size();
        if(!available) return {};

        // So we do have some data?
        auto box = buf.data();
        auto begin = asio::buffers_begin(box), end = asio::buffers_end(box);
        std::optional<TelnetMessage> response;
        bool escaped = false, match1 = false, match2 = false;

        // first, we read ahead
        if((uint8_t)*begin == IAC) {
            // If it begins with an IAC, then it's a Command, Negotiation, or Subnegotiation
            if(available < 2) {
                return {}; // not enough bytes available - do nothing;
            }
            // we have 2 or more bytes!
            auto b = begin;
            b++;
            auto sub = b;
            uint8_t option = 0;

            switch((uint8_t)*b) {
                case WILL:
                case WONT:
                case DO:
                case DONT:
                    // This is a negotiation.
                    if(available < 3) return {}; // negotiations require at least 3 bytes.
                    response.emplace(TelnetMsgType::Negotiation);
                    response.value().codes[0] = *b;
                    response.value().codes[1] = *(++b);
                    buf.consume(3);
                    return response;
                case SB:
                    // This is a subnegotiation. We need at least 5 bytes for it to work.
                    if(available < 5) return {};

                    option = *(++b);
                    b++;
                    sub = b;
                    // we must seek ahead until we have an unescaped IAC SE. If we don't have one, do nothing.

                    while(b != end) {
                        if(escaped) {
                            escaped = false;
                            b++;
                            continue;
                        }
                        if((uint8_t)*b == IAC) {
                            b++;
                            if(b != end && (uint8_t)*b == SE) {
                                // we have a winner!
                                response.emplace(TelnetMsgType::Subnegotiation);
                                response.value().codes[0] = option;
                                b--;
                                auto &vec = response.value().data;
                                std::copy(sub, b, std::back_inserter(vec));
                                buf.consume(5 + vec.size());
                                return response;
                            } else {
                                escaped = true;
                                b--;
                                continue;
                            }

                        } else {
                            b++;
                        }
                    }
                    // if we finished the while loop, we don't have enough data, so...
                    return {};
                default:
                    // if it's any other kind of IAC, it's a Command.
                    response.emplace(TelnetMsgType::Command);
                    response.value().data.push_back((uint8_t)*(++b));
                    buf.consume(2);
                    return response;
            };
        } else {
            // Data begins on something that isn't an IAC. Scan ahead until we reach one...
            // Send all data up to an IAC, or everything if there is no IAC, as data.
            response.emplace(TelnetMsgType::AppData);
            auto check = std::find(begin, end, IAC);
            auto &vec = response.value().data;
            std::copy(begin, check, std::back_inserter(vec));
            buf.consume(vec.size());
            return response;
        }
    }

    TelnetOption::TelnetOption(TelnetProtocol &prot, uint8_t code) : protocol(prot) {
        this->code = code;
    }

    uint8_t TelnetOption::opCode() const {
        return code;
    }

    bool TelnetOption::startDo() const {
        using namespace codes;
        switch(code) {
            case NAWS:
            case MTTS:
                return true;
            default:
                return false;
        }
    }

    bool TelnetOption::supportLocal() const {
        using namespace codes;
        switch(code) {
            //case MCCP2:
            case MSSP:
            case SGA:
            case MSDP:
            case GMCP:
            //case MXP:
                return true;
            default:
                return false;
        }
    }

    bool TelnetOption::supportRemote() const {
        using namespace codes;
        switch(code) {
            case NAWS:
            case MTTS:
                return true;
            default:
                return false;
        }
    }

    bool TelnetOption::startWill() const {
        using namespace codes;
        switch(code) {
            //case MCCP2:
            case MSSP:
            case SGA:
            case MSDP:
            case GMCP:
            //case MXP:
                return true;
            default:
                return false;
        }
    }

    void TelnetOption::enableLocal() {

    }

    void TelnetOption::enableRemote() {
        using namespace codes;
        switch(code) {
            case MTTS:
                protocol.conn.details.mtts = true;
                protocol.sendSub(code, std::vector<uint8_t>({1}));
                break;
        }
    }

    void TelnetOption::disableLocal() {

    }

    void TelnetOption::disableRemote() {

    }

    void TelnetOption::receiveNegotiate(uint8_t command) {
        using namespace codes;
        switch(command) {
            case WILL:
                if(supportRemote()) {
                    if(remote.negotiating) {
                        remote.negotiating = false;
                        if(!remote.enabled) {
                            remote.enabled = true;
                            enableRemote();
                            if(!remote.answered) {
                                remote.answered = true;
                            }
                        }
                    } else {
                        remote.enabled = true;
                        protocol.sendNegotiate(DO, opCode());
                        enableRemote();
                        if(!remote.answered) {
                            remote.answered = true;
                        }
                    }
                } else {
                    protocol.sendNegotiate(DONT, opCode());
                }
                break;
            case DO:
                if(supportLocal()) {
                    if(local.negotiating) {
                        local.negotiating = false;
                        if(!local.enabled) {
                            local.enabled = true;
                            enableLocal();
                            if(!local.answered) {
                                local.answered = true;
                            }
                        }
                    } else {
                        local.enabled = true;
                        protocol.sendNegotiate(WILL, opCode());
                        enableLocal();
                        if(!local.answered) {
                            local.answered = true;
                        }
                    }
                } else {
                    protocol.sendNegotiate(WONT, opCode());
                }
                break;
            case WONT:
                if(remote.enabled) disableRemote();
                if(remote.negotiating) {
                    remote.negotiating = false;
                    if(!remote.answered) {
                        remote.answered = true;
                    }
                }
                break;
            case DONT:
                if(local.enabled) disableLocal();
                if(local.negotiating) {
                    local.negotiating = false;
                    if(!local.answered) {
                        local.answered = true;
                    }
                }
                break;
            default:
                break;
        }
    }

    void TelnetOption::subNegotiate(const TelnetMessage &msg) {
        using namespace codes;
        switch(code) {
            case MTTS:
                subMTTS(msg);
                break;
        }
    }

    void TelnetOption::subMTTS(const TelnetMessage &msg) {
        if(msg.data.empty()) return; // we need data to be useful.
        if(msg.data[0] != 0) return; // this is invalid MTTS.
        if(msg.data.size() < 2) return; // we need at least some decent amount of data to be useful.

        std::string mtts;
        auto begin = msg.data.begin();
        begin++;
        // fill that string up and uppercase it.
        std::copy(begin, msg.data.end(), std::back_inserter(mtts));
        std::transform(mtts.begin(), mtts.end(), mtts.begin(), ::toupper);

        if(mtts == mtts_last) // there is no more data to be gleaned from asking...
            return;

        switch(mtts_count) {
            case 0:
                subMTTS_0(mtts);
                break;
            case 1:
                subMTTS_1(mtts);
                break;
            case 2:
                subMTTS_2(mtts);
                break;
        }

        mtts_count++;
        // cache the results and request more info.
        mtts_last = mtts;
        if(mtts_count >= 2) return; // there is no more info to request.
        protocol.sendSub(code, std::vector<uint8_t>({1}));

    }

    void TelnetOption::subMTTS_0(const std::string mtts) {
        auto first_space = std::find(mtts.begin(), mtts.end(), " ");
        if(first_space != mtts.end()) {
            protocol.conn.details.clientName.clear();
            std::copy(mtts.begin(), first_space-1, std::back_inserter(protocol.conn.details.clientName));

            protocol.conn.details.clientVersion.clear();
            std::copy(first_space+1, mtts.end(), std::back_inserter(protocol.conn.details.clientVersion));
        } else {
            protocol.conn.details.clientName = mtts;
        }

        auto &details = protocol.conn.details;
        auto &name = details.clientName;
        auto &version = details.clientVersion;

        if((name == "ATLANTIS") || (name == "CMUD") || (name == "KILDCLIENT") || (name == "MUDLET") ||
            (name == "PUTTY") || (name == "BEIP") || (name == "POTATO") || (name == "TINYFUGUE") || (name == "MUSHCLIENT")) {
            details.colorType = ring::net::XtermColor;
        }

        // all clients that support MTTS probably support ANSI...
        if(!details.colorType) {
            details.colorType = ring::net::StandardColor;
        }
    }

    void TelnetOption::subMTTS_1(const std::string mtts) {
        // the second MTTS negotiation gives the terminal type.
        auto sep = std::find(mtts.begin(), mtts.end(), "-");
        std::string termtype, extra;

        if(sep != mtts.end()) {
            std::copy(mtts.begin(), sep-1, std::back_inserter(termtype));
            std::copy(sep+1, mtts.end(), std::back_inserter(extra));
        } else {
            termtype = mtts;
        }

        auto &details = protocol.conn.details;

        if(termtype == "ANSI") {
            if(!details.colorType) {
                details.colorType = ring::net::StandardColor;
            }
        } else if (termtype == "VT100") {
            if(!details.colorType) {
                details.colorType = ring::net::StandardColor;
            }
            details.vt100 = true;
        } else if(termtype == "XTERM") {
            details.colorType = ring::net::XtermColor;
            details.vt100 = true;
        }

        if(extra == "256COLOR") {
            details.colorType = ring::net::XtermColor;
        } else if (extra == "TRUECOLOR") {
            details.colorType = ring::net::TrueColor;
        }
    }

    void TelnetOption::subMTTS_2(const std::string mtts) {
        auto sep = std::find(mtts.begin(), mtts.end(), " ");
        if(sep == mtts.end()) return;

        std::string mt, val;
        std::copy(mtts.begin(), sep-1, std::back_inserter(mt));
        std::copy(sep+1, mtts.end(), std::back_inserter(val));

        if(mt != "MTTS") return;

        int v = atoi(val.c_str());

        auto &details = protocol.conn.details;

        // ANSI
        if(v & 1) {
            if(!details.colorType) {
                details.colorType = ring::net::StandardColor;
            }
        }

        // VT100
        if(v & 2) {
            details.vt100 = true;
        }

        // UTF8
        if(v & 4) {
            details.utf8 = true;
        }

        // XTERM256 colors
        if(v & 8) {
            if(details.colorType < ring::net::XtermColor) {
                details.colorType = ring::net::XtermColor;
            }
        }

        // MOUSE TRACKING - who even uses this?
        if(v & 16) {
            details.mouse_tracking = true;
        }

        // On-screen color palette - again, is this even used?
        if(v & 32) {
            details.osc_color_palette = true;
        }

        // client uses a screen reader - this is actually somewhat useful for blind people...
        // if the game is designed for it...
        if(v & 64) {
            details.screen_reader = true;
        }

        // PROXY - I don't think this actually works?
        if(v & 128) {
            details.proxy = true;
        }

        // TRUECOLOR - support for this is probably rare...
        if(v & 256) {
            details.colorType = ring::net::TrueColor;
        }

        // MNES - Mud New Environment Standard support.
        if(v & 512) {
            details.mnes = true;
        }

        // mud server link protocol ???
        if(v & 1024) {
            details.mslp = true;
        }

    }

    void TelnetOption::load(nlohmann::json& j) {
        local.enabled = j["local"]["enabled"];
        local.negotiating = j["local"]["negotiating"];
        local.answered = j["local"]["answered"];

        remote.enabled = j["remote"]["enabled"];
        remote.negotiating = j["remote"]["negotiating"];
        remote.answered = j["remote"]["answered"];
    }

    nlohmann::json TelnetOption::serialize() const {
        nlohmann::json j;
        j["local"] = {
                {"enabled", local.enabled},
                {"negotiating", local.negotiating},
                {"answered", local.answered}
        };
        j["remote"] = {
                {"enabled", remote.enabled},
                {"negotiating", remote.negotiating},
                {"answered", remote.answered}
        };
        return j;
    }


    TelnetProtocol::TelnetProtocol(net::connection_details &det) : conn(det), start_timer(*net::executor, asio::chrono::milliseconds(300)) {
        using namespace codes;

        for(const auto &code : {MSSP, SGA, MSDP, GMCP, NAWS, MTTS}) {
            handlers.emplace(code, TelnetOption(*this, code));
        }
    }

    void TelnetProtocol::onConnect() {
        using namespace codes;
        for(auto &h : handlers) {
            if(h.second.startWill()) {
                h.second.local.negotiating = true;
                sendNegotiate(WILL, h.first);
            }
            if(h.second.startDo()) {
                h.second.remote.negotiating = true;
                sendNegotiate(DO, h.first);
            }
        }
        start_timer.async_wait([&](auto ec){if(!ec) start();});
    }

    void TelnetProtocol::start() {
        conn.onReady();
    }

    void TelnetProtocol::handleMessage(const TelnetMessage &msg) {
        switch(msg.msg_type) {
            case AppData:
                handleAppData(msg);
                break;
            case Command:
                handleCommand(msg);
                break;
            case Negotiation:
                handleNegotiate(msg);
                break;
            case Subnegotiation:
                handleSubnegotiate(msg);
                break;
        }
    }

    void TelnetProtocol::handleAppData(const TelnetMessage &msg) {

        for(const auto& c : msg.data) {
            switch(c) {
                case '\n':
                    conn.receiveText(app_data, net::Line);
                    app_data.clear();
                    break;
                case '\r':
                    // we just ignore these.
                    break;
                default:
                    app_data.push_back(c);
                    break;
            }
        }
    }

    void TelnetProtocol::handleCommand(const TelnetMessage &msg) {

    }

    void TelnetProtocol::handleNegotiate(const TelnetMessage &msg) {
        using namespace codes;
        if(!handlers.count(msg.codes[1])) {
            switch(msg.codes[0]) {
                case WILL:
                    sendNegotiate(DONT, msg.codes[1]);
                    break;
                case DO:
                    sendNegotiate(WONT, msg.codes[1]);
                    break;
                default:
                    break;
            }
            return;
        }
        auto &hand = handlers.at(msg.codes[1]);
        hand.receiveNegotiate(msg.codes[0]);
    }

    void TelnetProtocol::handleSubnegotiate(const TelnetMessage &msg) {
        if(handlers.count(msg.codes[0])) {
            auto &hand = handlers.at(msg.codes[0]);
            hand.subNegotiate(msg);
        }
    }

    void TelnetProtocol::sendNegotiate(uint8_t command, const uint8_t option) {
        std::vector<uint8_t> data = {codes::IAC, command, option};
        sendBytes(data);
    }

    void TelnetProtocol::sendText(const std::string &txt) {
        std::vector<uint8_t> data;
        for(const auto &c : txt) {
            switch(c) {
                case '\r':
                    break;
                case '\n':
                    data.push_back('\r');
                    data.push_back('\n');
                    break;
                default:
                    data.push_back(c);
                    break;
            }
        }
        sendBytes(data);
    }

    void TelnetProtocol::sendLine(const std::string &txt) {
        if(txt.empty()) {
            sendText("\n");
            return;
        }

        if(txt[txt.size()-1] == '\n') {
            sendText(txt);
            return;
        }
        sendText(txt + "\n");
        return;
    }

    void TelnetProtocol::sendPrompt(const std::string &prompt) {
        sendText(prompt);
    }

    void TelnetProtocol::sendBytes(const std::vector<uint8_t> &data) {

        if(conn.details.clientType == ring::net::TcpTelnet || conn.details.clientType == ring::net::TlsTelnet) {
            conn.buffers->write(data);

            if(conn.details.clientType == ring::net::TcpTelnet) {
                conn.plainSocket->send();
            }

        }
    }

    void TelnetProtocol::sendSub(const uint8_t op, const std::vector<uint8_t> &data) {
        using namespace codes;
        std::vector<uint8_t> out({IAC, SB, op});
        std::copy(data.begin(), data.end(), std::back_inserter(out));
        out.push_back(IAC);
        out.push_back(SE);
        sendBytes(out);
    }

    nlohmann::json TelnetProtocol::serialize() {
        nlohmann::json j;

        j["app_data"] = app_data;
        j["handlers"] = serializeHandlers();
        return j;
    }

    nlohmann::json TelnetProtocol::serializeHandlers() {
        auto j = nlohmann::json::array();

        for(const auto& h : handlers) {
            nlohmann::json j2 = {
                    {"code", h.first},
                    {"data", h.second.serialize()}
            };
            j.push_back(j2);
        }

        return j;
    }

}