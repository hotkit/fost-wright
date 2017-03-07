/*
    Copyright 2017, Felspar Co Ltd. http://support.felspar.com/
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#pragma once


#include <fost/rask/protocol>


namespace wright {


    namespace packet {
        /// In bound packet
        class in;
    }


    /// Hold the connection state
    class connection final :
        public rask::tcp_connection,
        public std::enable_shared_from_this<connection> {
    public:
        /// Create a connection that can be used to accept inbound connections
        connection(boost::asio::io_service &ios);

    protected:
        /// Process inbound messages
        void process_inbound(boost::asio::yield_context &) override;
        /// The outbound message stream
        void process_outbound(boost::asio::yield_context &) override;
        /// The connection has been established
        void established() override;
    };


    /// The protocol description for Wright
    using protocol_definition = rask::protocol<std::function<
        void(std::shared_ptr<connection>, rask::tcp_decoder&)>>;
    extern protocol_definition g_proto;


    /// Listen for inbound connections. The `listen` service is used for
    /// accepting connection and the `socket` service is used for the
    /// subsequent sockets.
    void start_server(boost::asio::io_service &listen, boost::asio::io_service &socket,
        uint16_t port);

    /// Keep a connection to a server open
    std::shared_ptr<connection> connect_to_server(boost::asio::io_service &ios, fostlib::host);


}

