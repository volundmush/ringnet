//
// Created by volund on 6/7/22.
//

#include "ringnet/connection.h"

namespace ring::net {

    MudConnection::MudConnection(std::string &conn_id, boost::asio::io_context &con) : conn_strand(con), conn_id(conn_id),
    game_messages(128) {}

    MudConnection::MudConnection(std::string &conn_id, boost::asio::io_context &con, nlohmann::json &j) : MudConnection(conn_id, con) {
        loadJson(j);
    }

    void MudConnection::sendLine(const std::string &txt) {
        sendText(txt, Line);
    }

    void MudConnection::sendPrompt(const std::string &txt) {
        sendText(txt, Prompt);
    }

    nlohmann::json MudConnection::serialize() {
        nlohmann::json j;
        j["details"] = details.serialize();
        j["conn_id"] = conn_id;

        return j;
    }

    void MudConnection::loadJson(nlohmann::json &j) {
        details.load(j["details"]);
        conn_id = j["conn_id"];
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

}