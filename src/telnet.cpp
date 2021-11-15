//
// Created by volund on 11/12/21.
//

#include <iostream>
#include <chrono>
#include "telnet.h"
#include "net.h"

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
                    sub = ++b;
                    // we must seek ahead until we have an unescaped IAC SE. If we don't have one, do nothing.

                    while(b != end) {
                        switch((uint8_t)*b) {
                            case IAC:
                                if(match1) {
                                    escaped = true;
                                    match1 = false;
                                } else {
                                    match1 = true;
                                }
                                break;
                            case SB:
                                if(!match1) {
                                    break;
                                }
                                // we have a winner!;
                                b--;
                                response.emplace(TelnetMsgType::Subnegotiation);
                                response.value().codes[0] = option;
                                std::copy(sub, b, response.value().data.begin());
                                buf.consume(5 + response.value().data.size());
                                return response;
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

    }

    TelnetProtocol::TelnetProtocol(net::connection_details &det) : conn(det), start_timer(net::executor, asio::chrono::milliseconds(300)) {
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
                    conn.in_mutex.lock();
                    std::cout << "Received a command: " << app_data << std::endl;
                    conn.text_in.emplace(app_data);
                    conn.in_mutex.unlock();
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
        if(!handlers.contains(msg.data[1])) {
            switch(msg.data[0]) {
                case WILL:
                    sendNegotiate(DONT, msg.data[1]);
                    break;
                case DO:
                    sendNegotiate(WONT, msg.data[1]);
                    break;
                default:
                    break;
            }
            return;
        }
        auto hand = handlers.at(msg.data[1]);
        hand.receiveNegotiate(msg.data[0]);
    }

    void TelnetProtocol::handleSubnegotiate(const TelnetMessage &msg) {
        if(handlers.contains(msg.data[0])) {
            auto hand = handlers.at(msg.data[0]);
            hand.subNegotiate(msg);
        }
    }

    void TelnetProtocol::sendNegotiate(uint8_t command, const uint8_t option) {
        std::vector<uint8_t> data = {codes::IAC, command, option};
        sendBytes(data);
    }

    void TelnetProtocol::sendBytes(const std::vector<uint8_t> &data) {

        if(conn.details.clientType == ring::net::TcpTelnet || conn.details.clientType == ring::net::TlsTelnet) {
            auto &out_buffer = conn.buffers->out_buffer;
            auto prep = out_buffer.prepare(data.size());
            memcpy(prep.data(), data.data(), data.size());
            out_buffer.commit(data.size());

            if(conn.details.clientType == ring::net::TcpTelnet) {
                conn.plainSocket->send();
            }

        }
    }

}