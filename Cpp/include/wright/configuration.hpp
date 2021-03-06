/**
    Copyright 2017-2019 Red Anchor Trading Co. Ltd.

    Distributed under the Boost Software License, Version 1.0.
    See <http://www.boost.org/LICENSE_1_0.txt>
 */


#pragma once


#include <fost/core>


namespace wright {


    /// Modules describing the system
    extern const fostlib::module c_wright;
    extern const fostlib::module c_exec_helper;


    /// The child number. Zero means the parent process
    extern const fostlib::setting<int64_t> c_child;
    /// The number of children to spawn
    extern const fostlib::setting<int64_t> c_children;
    /// The file descriptor to use for resend notificaitons
    extern const fostlib::setting<int> c_resend_fd;
    /// The child program to execute
    extern const fostlib::setting<fostlib::json> c_exec;

    /// The port for the server
    extern const fostlib::setting<uint16_t> c_port;
    /// The netloc to connect to (instead of reading from stdin)
    extern const fostlib::setting<fostlib::nullable<fostlib::string>> c_connect;
    /// Target overspill capacity per worker. This should be used to account
    /// for extra network latency. Increase as appropriate to prevent work
    /// stalls.
    extern const fostlib::setting<std::size_t> c_overspill_cap_per_worker;

    /// Whether to simulate
    extern const fostlib::setting<bool> c_simulate;
    /// Set to false to stop the simulated worker from crashing
    extern const fostlib::setting<bool> c_can_die;
    /// Mean time for simulated jobs
    extern const fostlib::setting<unsigned> c_sim_mean;
    /// Standard deviation for simulated jobs
    extern const fostlib::setting<unsigned> c_sim_sd;


}
