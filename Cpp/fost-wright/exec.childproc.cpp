/*
    Copyright 2017, Felspar Co Ltd. http://support.felspar.com/
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include <wright/configuration.hpp>
#include <wright/exec.childproc.hpp>

#include <fost/log>

#include <iostream>

#include <sys/wait.h>


void wright::fork_worker() {
    std::vector<char const *> argv;
    const auto command = c_exec.value();
    argv.push_back(command.c_str());
    argv.push_back("-x"); // Simulate
    argv.push_back("true");
    argv.push_back("-b"); // No banner
    argv.push_back("false");
    argv.push_back(nullptr);
    /// Fork and loop until done
    while ( true ) {
        int pid = ::fork();
        if ( pid < 0 ) {
            fostlib::log::error(c_exec_helper)
                ("", "Fork failed")
                ("parent", ::getpid());
            exit(5);
        } else if ( pid == 0 ) {
            ::execvp(argv[0], const_cast<char *const*>(argv.data()));
        } else {
            fostlib::log::info(c_exec_helper)
                ("", "Started child process")
                ("pid", pid)
                ("resend-fd", wright::c_resend_fd.value());
            int status;
            auto waited = waitpid(pid, &status, 0);
            if ( status == 0 ) {
                fostlib::log::info(c_exec_helper)
                    ("", "Child completed")
                    ("pid", pid);
                break;
            }
            auto logger = fostlib::log::warning(c_exec_helper);
            logger
                ("", "Child errored")
                ("pid", pid);
                ("status", status);
            /// Send a resend instruction to the parent process
            const char resend[] = "r";
            /// Write a single byte into the pipe
            logger("Requested resend", ::write(wright::c_resend_fd.value(), resend, 1u) == 1);
        }
    }
}


/*
 * wright::childproc
 */


namespace {
    const struct newl {
        newl() {
            buffer.sputc('\n');
        }
        boost::asio::streambuf buffer;
    } newline;
}


void wright::childproc::write(
    boost::asio::io_service &ios,
    const std::string &command,
    boost::asio::yield_context &yield
) {
    std::array<boost::asio::streambuf::const_buffers_type, 2>
        buffer{{{command.data(), command.size()}, newline.buffer.data()}};
    boost::asio::async_write(stdin.parent(ios), buffer, yield);
}


std::string wright::childproc::read(
    boost::asio::io_service &ios,
    boost::asio::streambuf &buffer,
    boost::asio::yield_context yield
) {
    boost::system::error_code error;
    auto bytes = boost::asio::async_read_until(stdout.parent(ios), buffer, '\n', yield[error]);
    if ( error ) {
        std::cerr << pid << " read error: " << error << std::endl;
    } else if ( bytes ) {
        /// For some reason I totally fail to understand we can actually
        /// end up reading a sequence of zero byte values at the start
        /// of the line. These need to be filtered out as they appear to
        /// be totally spurious. They fail the string comparison when we
        /// check if the line read matches the job we sent.
        ///
        /// At least that can happen if we give the buffer to a
        /// `std::istream`. It doesn't seem to happen when pulling the
        /// characters off one at a time.
        std::string line;
        line.reserve(bytes);
        for ( ; bytes; --bytes ) {
            char next = buffer.sbumpc();
            if ( next == 0 || next == '\n' ) {
//                 std::cerr << pid << " dropped " << int(next) << std::endl;
            } else {
                line += next;
            }
        }
        return line;
    }
    return std::string();
}


void wright::childproc::close() {
    stdout.close();
    stdin.close();
}
