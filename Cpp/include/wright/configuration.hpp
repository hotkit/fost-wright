/*
    Copyright 2017, Felspar Co Ltd. http://support.felspar.com/
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
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
    /// Whether to simulate
    extern const fostlib::setting<bool> c_simulate;
    /// The file descriptor to use for resend notificaitons
    extern const fostlib::setting<int> c_resend_fd;
    /// The child program to execute
    extern const fostlib::setting<fostlib::string> c_exec;


}
