/*
 * Copyright 2023, MongoDB, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the “License”);
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

#include <csignal>
#include <iostream>
#include <sstream>
#include <string>

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/program_options.hpp>

#include "lines.h"
#include "session.h"

static constexpr auto kVersion = "0.2";

namespace po = boost::program_options;
namespace logging = boost::log;

void signal_handler(int signal) {
    BOOST_LOG_TRIVIAL(debug) << "Received signal: " << signal;
    BOOST_LOG_TRIVIAL(debug) << "Exiting early";
    throw exception::early_exit();
}

void setup_signal_handler(int signal, const std::string& name) {
    if (std::signal(signal, signal_handler) == SIG_ERR) {
        std::ostringstream oss;
        oss << "Could not setup " << name << " signal handler";
        throw std::runtime_error(oss.str());
    }
}

static po::options_description options("Options");
static const char* cmd_name = "gupty";

void show_version() {
    std::cout << "gupty version " << kVersion << std::endl;
}

void show_help() {
    show_version();
    std::cout << "Usage: " << cmd_name << " [OPTIONS] <script-file.gupty>" << std::endl;
    std::cout << options << std::endl;
}

static constexpr auto kOptHelp = "help";
static constexpr auto kOptDebug = "debug";
static constexpr auto kOptVersion = "version";
static constexpr auto kOptScriptFile = "script-file";
static constexpr auto kOptShell = "shell";
static constexpr auto kOptLogFile = "log-file";
static constexpr auto kOptMonitorFile = "monitor-file";

int main(int argc, char *argv[]) {
    int rc = 0;
    try {
        if (argv[0]) {
            cmd_name = argv[0];
        }

        options.add_options()
            ("version,v"         , "show version")
            ("help,h"            , "print help message")
            ("debug,d"           , "debug mode, log everything")
            (kOptShell           , po::value<std::string>()->default_value(""), "use shell instead of default")
            (kOptLogFile         , po::value<std::string>()->default_value("gupty.log"), "log file name")
            (kOptMonitorFile     , po::value<std::string>()->default_value(".gupty.monitor"), "monitor file name")
            (kOptScriptFile      , po::value<std::string>(), "script file to use")
            ;

        po::positional_options_description args;
        args.add(kOptScriptFile, 1);

        po::variables_map vm;
        po::store(po::command_line_parser(argc, argv).options(options).positional(args).run(), vm);
        po::notify(vm);

        if (vm.count(kOptVersion)) {
            show_version();
            exit(0);
        }

        if (vm.count(kOptHelp) || vm.count(kOptScriptFile) == 0) {
            show_help();
            exit(0);
        }

        logging::add_file_log(vm[kOptLogFile].as<std::string>());
        if (vm.count(kOptDebug)) {
            logging::core::get()->set_filter(logging::trivial::severity >= logging::trivial::debug);
            BOOST_LOG_TRIVIAL(debug) << "Logging level set to 'debug'";
        } else {
            logging::core::get()->set_filter(logging::trivial::severity >= logging::trivial::error);
        }

        setup_signal_handler(SIGINT, "SIGINT");
        setup_signal_handler(SIGQUIT, "SIGQUIT");

        Session session;
        auto cmds = session.resolveCommands(readLines(vm[kOptScriptFile].as<std::string>()));
        session.setMonitor(vm[kOptMonitorFile].as<std::string>());
        session.setShell(vm[kOptShell].as<std::string>());
        session.init();
        session.run(cmds);

    } catch(const exception::normal_exit& e) {
        BOOST_LOG_TRIVIAL(debug) << "Exiting normally.";
        rc = 0;

    } catch(const exception::early_exit& e) {
        BOOST_LOG_TRIVIAL(debug) << "Exiting early.";
        rc = 1;

    } catch(const std::runtime_error& e) {
        std::cerr << argv[0] << ": Runtime error: " << e.what() << std::endl;
        BOOST_LOG_TRIVIAL(error) << "Runtime error: " << e.what();
        rc = 2;

    } catch(const std::exception& e) {
        std::cerr << argv[0] << ": Error: " << e.what() << std::endl;
        BOOST_LOG_TRIVIAL(error) << "Error: " << e.what();
        rc = 2;

    } catch(...) {
        std::cerr << argv[0] << ": Unknown error." << std::endl;
        BOOST_LOG_TRIVIAL(error) << "Unknown error.";
        rc = 3;
    }

    std::cout << std::endl;
    std::cout << "[exited gupty]" << std::endl;

    return rc;
}
