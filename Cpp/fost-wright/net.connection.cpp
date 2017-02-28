/*
    Copyright 2017, Felspar Co Ltd. http://support.felspar.com/
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include <wright/configuration.hpp>
#include <wright/net.packets.hpp>
#include <wright/net.server.hpp>

#include <fost/log>

#include <mutex>


namespace {


    std::mutex g_mutex;
    std::vector<std::weak_ptr<wright::connection>> g_connections;

    void live(std::shared_ptr<wright::connection> ptr) {
        std::unique_lock<std::mutex> lock(g_mutex);
        /// Go through and put this connection in either a spare slot
        for ( auto &w : g_connections ) {
            auto slot(w.lock());
            if ( not slot ) {
                w = ptr;
                return;
            }
        }
        /// or append it to the end
        g_connections.push_back(ptr);
    }


}


wright::connection::connection(boost::asio::io_service &ios)
: tcp_connection(ios) {
}


wright::connection::connection(boost::asio::io_service &ios, fostlib::host endpoint)
: connection(ios) {
    /// Try to connect to the remote server
    boost::asio::ip::tcp::resolver resolver{ios};
    boost::asio::ip::tcp::resolver::query q(endpoint.name().c_str(), endpoint.service().value().c_str());
    auto endp = resolver.resolve(q);
    socket.open(endp->endpoint().protocol());
    fostlib::log::debug(wright::c_exec_helper)
        ("", "Connection established")
        ("host", endpoint);
}


void wright::connection::process_inbound(boost::asio::yield_context &yield) {
    auto self = shared_from_this();
    receive_loop(*this, yield,
        [&](auto decode, uint8_t control, std::size_t bytes)
    {
        g_proto.dispatch(version(), control, self, decode);
    });
}


void wright::connection::process_outbound(boost::asio::yield_context  &yield) {
//     while ( socket->is_open() ) {
//     }
}


void wright::connection::established() {
    live(shared_from_this());
}

