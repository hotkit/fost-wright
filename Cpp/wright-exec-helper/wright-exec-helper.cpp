/*
    Copyright 2016-2017, Felspar Co Ltd. http://support.felspar.com/
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#define BOOST_COROUTINES_NO_DEPRECATION_WARNING
#define BOOST_COROUTINE_NO_DEPRECATION_WARNING

#include <wright/childproc.hpp>
#include <wright/echo.hpp>

#include <f5/threading/boost-asio.hpp>
#include <fost/cli>
#include <fost/main>
#include <fost/unicode>

#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>

#include <future>

#include <sys/wait.h>

#include <cxxabi.h>


namespace {
    /// The child number. Zero means the parent process
    const fostlib::setting<int64_t> c_child(__FILE__, "wright-exec-helper",
        "Child", 0, true);
    const fostlib::setting<int64_t> c_children(__FILE__, "wright-exec-helper",
        "Children", std::thread::hardware_concurrency(), true);


    const auto exception_decorator = [](auto fn/*, std::function<void(void)> recov = [](){}*/) {
        return [=](auto &&...a) {
                try {
                    fn(a...);
                } catch ( boost::coroutines::detail::forced_unwind & ) {
                    throw;
                } catch ( std::exception &e ) {
                    std::cerr << e.what() << ": " << c_child.value() << std::endl;
                    throw;
                } catch ( ... ) {
                    std::cerr << "Unkown exception: " << c_child.value() << " - "
                        << __cxxabiv1::__cxa_current_exception_type()->name() << std::endl;
                    throw;
                }
            };
        };
}


FSL_MAIN(
    L"wright-exec-helper",
    L"Wright execution helper\nCopyright (c) 2016, Felspar Co. Ltd."
)( fostlib::ostream &out, fostlib::arguments &args ) {
    /// Configure settings
    const fostlib::setting<fostlib::string> c_exec(__FILE__, "wright-exec-helper",
        "Execute", args[0].value(), true);
    args.commandSwitch("c", c_child);
    args.commandSwitch("w", c_children);

    if ( c_child.value() ) {
        /// Child process needs to do the right thing
        wright::echo(std::cin, out, std::cerr);
    } else {
        /// The parent sets up the communications redirects etc and spawns
        /// child processes
        std::vector<wright::childproc> children(c_children.value());

        /// Set up the argument vector for the child
        const auto command = c_exec.value();
        std::vector<char const *> argv;
        argv.push_back(command.c_str());
        if ( command == argv[0] ) {
            argv.push_back("-c");
            argv.push_back(nullptr); // holder for the child number
            argv.push_back("-b"); // No banner
            argv.push_back("false");
        }
        argv.push_back(nullptr);

        /// For each child go through and fork and execvpe it
        for ( auto child = 0; child < c_children.value(); ++child ) {
            auto child_number = std::to_string(child + 1);
            argv[2] = child_number.c_str();
            children[child].pid = fork();
            if ( children[child].pid < 0 ) {
                throw std::system_error(errno, std::system_category());
            } else if ( children[child].pid == 0 ) {
                dup2(children[child].stdin.child(), STDIN_FILENO);
                dup2(children[child].stdout.child(), STDOUT_FILENO);
                children.clear(); // closes the pipes in this process
                execvp(argv[0], const_cast<char *const *>(argv.data()));
            }
        }

        /// Set up a promise that we're going to wait to finish on
        std::promise<void> blocker;
        /// Flag to tell us if the input stream has completed (closed)
        /// yet and if the blocker has been signalled
        bool in_closed{false}, signalled{false};

        /// Stop on exception, one thread. We want one thread here so
        /// we don't have to worry about thread synchronisation when
        /// running code in the reactor
        f5::boost_asio::reactor_pool coordinator([]() { return false; }, 1u);
        auto &ios = coordinator.get_io_service();

        /// All the children need a presence in the reactor pool for
        /// their process requirement
        for ( auto &child : children ) {
            /// Each child will wait on the command, then write it
            /// the pipe for the process to execute and wait on the result
            auto *cp = &child;
            boost::asio::spawn(ios, exception_decorator([&, cp](auto yield) {
                    /// Wait for a job to be queued in their process ID
                    boost::asio::streambuf buffer;
                    while ( cp->stdout.parent(ios).is_open() ) {
                        boost::system::error_code error;
                        auto ret = cp->read(ios, buffer, yield[error]);
                        if ( not error && not ret.empty() ) {
                            cp->commands.pop_front();
                            out << ret << std::endl;
                            if ( in_closed && not signalled ) {
                                const auto working = std::count_if(children.begin(), children.end(),
                                        [](const auto &c) { return not c.commands.empty(); });
                                if ( working == 0 ) {
                                    signalled = true;
                                    blocker.set_value();
                                }
                            }
                        }
                    }
                }));
        }

        /// This process now needs to read from stdin and queue the jobs
        boost::asio::posix::stream_descriptor as_stdin(ios);
        {
            boost::system::error_code error;
            as_stdin.assign(dup(STDIN_FILENO), error);
            if ( error ) {
                std::cerr << "Cannot assign stdin to the reactor pool. This "
                    "probably means you're trying to redirect a file rather "
                    "than pipe the commands\n\nI.e. try this:\n"
                    "   cat commands.txt | wright-exec-helper\n"
                    "instead of\n"
                    "   wright-exec-helper < commands.txt" << std::endl;
                std::exit(2);
            }
        }
        boost::asio::spawn(ios, exception_decorator([&](auto yield) {
                f5::eventfd::limiter limit{ios, yield, children.size() * wright::buffer_size};
                boost::asio::streambuf buffer;
                while ( as_stdin.is_open() ) {
                    boost::system::error_code error;
                    auto bytes = boost::asio::async_read_until(as_stdin, buffer, '\n', yield[error]);
                    if ( error ) {
                        in_closed = true;
                        return;
                    } else if ( bytes ) {
                        buffer.commit(bytes);
                        std::istream in(&buffer);
                        std::string line;
                        std::getline(in, line);
                        auto task = ++limit; // Must do this first so it can block
                        for ( auto &child : children ) {
                            if ( not child.commands.full() ) {
                                child.write(ios, line, yield);
                                child.commands.push_back(std::make_pair(line, std::move(task)));
                                break;
                            }
                        }
                    }
                }
            }));

        /// This needs to block here until all processing is done
        auto blocker_ready = blocker.get_future();
        blocker_ready.wait();

        /// Terminating. Wait for children
        for ( auto &child : children ) {
            child.close();
            waitpid(child.pid, nullptr, 0);
        }
    }
    return 0;
}

