//
// Created by volund on 11/12/21.
//

#include <iostream>
#include <chrono>
#include "ringnet/telnet.h"
#include "ringnet/net.h"
#include "boost/algorithm/string.hpp"
#include "base64_default_rfc4648.hpp"


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

    opt_type<TelnetMessage> parse_message(boost::asio::streambuf &buf) {
        using namespace ring::telnet::codes;
        // return early if nothing to do.
        auto available = buf.size();
        if(!available) return {};

        // So we do have some data?
        auto box = buf.data();
        auto begin = boost::asio::buffers_begin(box), end = boost::asio::buffers_end(box);
        opt_type<TelnetMessage> response;
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

    TelnetOption::TelnetOption(MudTelnetConnection *prot, uint8_t code) : conn(prot) {
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
                conn->details.mtts = true;
                conn->sendSub(code, std::vector<uint8_t>({1}));
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
                        conn->sendNegotiate(DO, opCode());
                        enableRemote();
                        if(!remote.answered) {
                            remote.answered = true;
                        }
                    }
                } else {
                    conn->sendNegotiate(DONT, opCode());
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
                        conn->sendNegotiate(WILL, opCode());
                        enableLocal();
                        if(!local.answered) {
                            local.answered = true;
                        }
                    }
                } else {
                    conn->sendNegotiate(WONT, opCode());
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

        std::string mtts = boost::algorithm::to_upper_copy(std::string(msg.data.begin(), msg.data.end()).substr(1));

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
        conn->sendSub(code, std::vector<uint8_t>({1}));

    }

    void TelnetOption::subMTTS_0(const std::string& mtts) {
        std::vector<std::string> namecheck;
        auto to_check = boost::algorithm::to_upper_copy(mtts);
        boost::algorithm::split(namecheck, to_check, boost::algorithm::is_space());
        switch(namecheck.size()) {
            case 2:
                conn->details.clientVersion = namecheck[1];
            case 1:
                conn->details.clientName = namecheck[0];
                break;
        }

        auto &details = conn->details;
        auto &name = details.clientName;
        auto &version = details.clientVersion;

        if((name == "ATLANTIS") || (name == "CMUD") || (name == "KILDCLIENT") || (name == "MUDLET") ||
            (name == "PUTTY") || (name == "BEIP") || (name == "POTATO") || (name == "TINYFUGUE") || (name == "MUSHCLIENT")) {
            details.colorType = std::max(details.colorType, ring::net::XtermColor);
        }

        // all clients that support MTTS probably support ANSI...
        details.colorType = std::max(details.colorType, ring::net::StandardColor);
    }

    void TelnetOption::subMTTS_1(const std::string& mtts) {

        std::vector<std::string> splitcheck;
        auto to_check = boost::algorithm::to_upper_copy(mtts);
        boost::algorithm::split(splitcheck, to_check, boost::algorithm::is_any_of("-"));

        auto &details = conn->details;


        switch(splitcheck.size()) {
            case 2:
                if(splitcheck[1] == "256COLOR") {
                    details.colorType = std::max(details.colorType, ring::net::XtermColor);
                } else if (splitcheck[1] == "TRUECOLOR") {
                    details.colorType = std::max(details.colorType, ring::net::TrueColor);
                }
            case 1:
                if(splitcheck[0] == "ANSI") {
                    details.colorType = std::max(details.colorType, ring::net::StandardColor);
                } else if (splitcheck[0] == "VT100") {
                    details.colorType = std::max(details.colorType, ring::net::StandardColor);
                    details.vt100 = true;
                } else if(splitcheck[0] == "XTERM") {
                    details.colorType = std::max(details.colorType, ring::net::XtermColor);
                    details.vt100 = true;
                }
                break;
        }
    }

    void TelnetOption::subMTTS_2(const std::string mtts) {
        std::vector<std::string> splitcheck;
        auto to_check = boost::algorithm::to_upper_copy(mtts);
        boost::algorithm::split(splitcheck, to_check, boost::algorithm::is_space());

        if(splitcheck.size() < 2) return;

        if(splitcheck[0] != "MTTS") return;

        int v = atoi(splitcheck[1].c_str());

        auto &details = conn->details;

        // ANSI
        if(v & 1) {
            details.colorType = std::max(details.colorType, ring::net::StandardColor);
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
            details.colorType = std::max(details.colorType, ring::net::XtermColor);
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
            details.colorType = std::max(details.colorType, ring::net::TrueColor);
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


    MudTelnetConnection::MudTelnetConnection(std::string &conn_id, boost::asio::io_context &con) : ring::net::MudConnection(conn_id, con),
    start_timer(con, boost::asio::chrono::milliseconds(1000)) {
        using namespace codes;

        for(const auto &code : {MSSP, SGA, MSDP, GMCP, NAWS, MTTS}) {
            handlers.emplace(code, TelnetOption(this, code));
        }
    }

    MudTelnetConnection::MudTelnetConnection(std::string &conn_id, boost::asio::io_context &con, nlohmann::json &j) : MudTelnetConnection(conn_id, con) {}

    void MudTelnetConnection::onConnect() {
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
        start_timer.expires_after(boost::asio::chrono::milliseconds(300));
        start_timer.async_wait([&](auto ec){if(!ec) ready();});
    }

    void MudTelnetConnection::start() {
        onConnect();
    }

    void MudTelnetConnection::ready() {
        net::ConnectionMsg m;
        m.conn_id = conn_id;
        m.event = net::CONNECTED;
        net::manager.events.push(m);
    }

    void MudTelnetConnection::handleMessage(const TelnetMessage &msg) {
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

    void MudTelnetConnection::handleAppData(const TelnetMessage &msg) {
        net::GameMsg g;
        for(const auto& c : msg.data) {
            switch(c) {
                case '\n':
                    g.command = app_data;
                    app_data.clear();
                    game_messages.push(g);
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

    void MudTelnetConnection::handleCommand(const TelnetMessage &msg) {

    }

    void MudTelnetConnection::resume() {}

    void MudTelnetConnection::handleNegotiate(const TelnetMessage &msg) {
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

    void MudTelnetConnection::handleSubnegotiate(const TelnetMessage &msg) {
        if(handlers.count(msg.codes[0])) {
            auto &hand = handlers.at(msg.codes[0]);
            hand.subNegotiate(msg);
        }
    }

    void MudTelnetConnection::sendNegotiate(uint8_t command, const uint8_t option) {
        std::vector<uint8_t> data = {codes::IAC, command, option};
        sendBytes(data);
    }

    void MudTelnetConnection::sendText(const std::string &txt, net::TextType mode) {
        if(txt.empty()) return;
        std::vector<uint8_t> data;

        // standardize outgoing linebreaks for telnet.
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

        if(mode == net::Line && !boost::algorithm::ends_with(txt, "\n")) {
            data.push_back('\r');
            data.push_back('\n');
        }

        if(mode == net::Prompt) {
            data.push_back(codes::IAC);
            if(details.telopt_eor) data.push_back(codes::EOR); else data.push_back(codes::GA);
        }
        sendBytes(data);
    }

    void MudTelnetConnection::sendLine(const std::string &txt) {
        sendText(txt + "\n", net::Line);
    }

    void MudTelnetConnection::sendPrompt(const std::string &txt) {
        sendText(txt, net::Prompt);
    }

    void MudTelnetConnection::sendSub(const uint8_t op, const std::vector<uint8_t> &data) {
        using namespace codes;
        std::vector<uint8_t> out({IAC, SB, op});
        std::copy(data.begin(), data.end(), std::back_inserter(out));
        out.push_back(IAC);
        out.push_back(SE);
        sendBytes(out);
    }

    nlohmann::json MudTelnetConnection::serialize() {
        auto j = MudConnection::serialize();
        j["app_data"] = app_data;
        j["handlers"] = serializeHandlers();
        return j;
    }

    nlohmann::json MudTelnetConnection::serializeHandlers() {
        nlohmann::json j;
        for(const auto& h : handlers) {
            j.push_back(std::tuple(h.first, h.second.serialize()));
        }
        return j;
    }

    void MudTelnetConnection::loadJson(nlohmann::json &j) {
        MudConnection::loadJson(j);
        if(j.contains("app_data")) app_data = j["app_data"];
        if(j.contains("handlers")) for(auto &j2 : j["handlers"]) {
            uint8_t id = j2[0];
            auto handler = handlers.find(id);
            if(handler != handlers.end()) {
                handler->second.load(j2[1]);
            }
        }
    }

    void MudTelnetConnection::onDataReceived() {
        while(auto msg = parse_message(in_buffer)) {
            handleMessage(msg.value());
        }
    }

    void MudTelnetConnection::sendJson(const nlohmann::json &j) {

    }

    void MudTelnetConnection::sendMSSP(const std::vector<std::tuple<std::string, std::string>> &data) {

    }

    void MudTelnetConnection::onClose() {}

    TcpMudTelnetConnection::TcpMudTelnetConnection(std::string &conn_id, boost::asio::io_context &con) : MudTelnetConnection(conn_id, con), _socket(con) {}

    TcpMudTelnetConnection::TcpMudTelnetConnection(std::string &conn_id, boost::asio::io_context &con, nlohmann::json &j,
                                                   boost::asio::ip::tcp prot, int socket) : MudTelnetConnection(conn_id, con, j), _socket(con, prot, socket) {
        MudTelnetConnection::loadJson(j);
        std::string in_data = j["in_buffer"], out_data = j["out_buffer"];
        if(!in_data.empty()) {
            std::vector<uint8_t> in_d = base64::decode(in_data);
            auto prep = in_buffer.prepare(in_d.size());
            memcpy(prep.data(), in_d.data(), in_d.size());
            in_buffer.commit(in_d.size());
        }

        if(!out_data.empty()) {
            std::vector<uint8_t> out_d = base64::decode(out_data);
            auto prep = out_buffer.prepare(out_d.size());
            memcpy(prep.data(), out_d.data(), out_d.size());
            out_buffer.commit(out_d.size());
        }
    }

    nlohmann::json TcpMudTelnetConnection::serialize() {
        using base64 = cppcodec::base64_rfc4648;
        auto j = MudTelnetConnection::serialize();
        j["socket"] = _socket.native_handle();
        j["protocol"] = _socket.local_endpoint().protocol() == boost::asio::ip::tcp::v4() ? 4 : 6;
        if(in_buffer.size()) j["in_buffer"] = base64::encode((uint8_t*)in_buffer.data().data(), in_buffer.data().size());
        if(out_buffer.size()) j["out_buffer"] = base64::encode((uint8_t*)out_buffer.data().data(), out_buffer.data().size());
        if(ex_buffer.size()) j["ex_buffer"] = base64::encode((uint8_t*)ex_buffer.data().data(), ex_buffer.data().size());
        return j;
    }

    void TcpMudTelnetConnection::start() {
        conn_strand.post([this] { read(); });
        MudTelnetConnection::start();
        conn_strand.post([this] { write(); });
    }

    void TcpMudTelnetConnection::resume() {
        conn_strand.post([this] { read(); });
        conn_strand.post([this] { write(); });
    }

    void TcpMudTelnetConnection::do_read(boost::system::error_code ec, std::size_t trans) {
        if(ec) {
            // deal with this later.
        } else {
            // all is well, we got some data.
            in_buffer.commit(trans);
            onDataReceived();
            auto prep = in_buffer.prepare(1024);

            _socket.async_read_some(boost::asio::buffer(prep), [this](auto ec, std::size_t trans) { do_read(ec, trans); });
        }
    }

    void TcpMudTelnetConnection::read() {
        auto prep = in_buffer.prepare(1024);
        _socket.async_read_some(boost::asio::buffer(prep), [this](auto ec, std::size_t trans) { do_read(ec, trans); });
    }


    void TcpMudTelnetConnection::do_write(boost::system::error_code ec, std::size_t trans) {
        if(ec) {
            // deal with this later.
            }
        else {
            out_buffer.consume(trans);
            if(buf_mutex.try_lock()) {
                if(ex_buffer.size()) {
                    auto prep = out_buffer.prepare(ex_buffer.size());
                    memcpy(prep.data(), ex_buffer.data().data(), ex_buffer.size());
                    out_buffer.commit(ex_buffer.size());
                    ex_buffer.consume(ex_buffer.size());
                }
                buf_mutex.unlock();
            }

            if(out_buffer.size())
                _socket.async_write_some(out_buffer.data(), [this](auto ec, std::size_t trans) { do_write(ec, trans); });
            else {
                out_mutex.unlock();
                isWriting = false;
            }
        }
    }

    void TcpMudTelnetConnection::real_write() {
        out_mutex.lock();
        _socket.async_write_some(out_buffer.data(), [this](auto ec, std::size_t trans) { do_write(ec, trans); });
    }

    void TcpMudTelnetConnection::sendBytes(const std::vector<uint8_t> &data) {
        if(data.empty()) return;
        if(out_mutex.try_lock()) {
            auto prep = out_buffer.prepare(data.size());
            memcpy(prep.data(), data.data(), data.size());
            out_buffer.commit(data.size());
            out_mutex.unlock();
            write();
        } else {
            buf_mutex.lock();
            auto prep = ex_buffer.prepare(data.size());
            memcpy(prep.data(), data.data(), data.size());
            ex_buffer.commit(data.size());
            buf_mutex.unlock();
        }
    }

    void TcpMudTelnetConnection::write() {
        if(!isWriting) {
            if(out_buffer.size()) {
                isWriting = true;
                conn_strand.post([this]{ real_write(); });
            }
        }
    }
}