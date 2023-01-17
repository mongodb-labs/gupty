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

#include <algorithm>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>

#include <boost/lexical_cast.hpp>
#include <boost/log/trivial.hpp>
#include <boost/process.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <cstring>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/poll.h>
#include <sys/wait.h>

#include "keycodes.h"
#include "session.h"

constexpr auto CMD_NOTE = "note";
constexpr auto CMD_SKIP = "skip";
constexpr auto CMD_RESUME = "resume";
constexpr auto CMD_SET_MODE = "set_mode";
constexpr auto CMD_PAUSE = "pause";
constexpr auto CMD_OUTPUT = "output";
constexpr auto CMD_EXIT = "exit";
constexpr auto CMD_RUN = "run";

constexpr auto CMD_WAIT_FOR_ANY_KEY = "wait_for_any_key";
constexpr auto CMD_PASTE_KEYS = "paste_keys";
constexpr auto CMD_PASTE_KEY = "paste_key";
constexpr auto CMD_TYPE_KEYS = "type_keys";
constexpr auto CMD_TYPE_KEY = "type_key";
constexpr auto CMD_WAIT_FOR_ENTER = "wait_for_enter";
constexpr auto CMD_WAIT_FOR_AND_SEND_ENTER = "wait_for_and_send_enter";
constexpr auto CMD_PASTE = "paste";
constexpr auto CMD_PASTE_LINE = "paste_line";
constexpr auto CMD_TYPE_LINE = "type_line";
constexpr auto CMD_TYPE = "type";

// FIXME: these should go away in favour of UserInputModeNames
constexpr auto MODE_INSERT = "insert";
constexpr auto MODE_COMMAND = "command";
constexpr auto MODE_PASSTHROUGH = "passthrough";
constexpr auto MODE_AUTO = "auto";

// FIXME: these should go away in favour of OutputModeNames
constexpr auto OUTPUT_ALL = "all";
constexpr auto OUTPUT_NONE = "none";


Enum<Session::UserInputMode> Session::UserInputModeNames({
    {"COMMAND", UserInputMode::COMMAND},
    {"INSERT", UserInputMode::INSERT},
    {"PASSTHROUGH", UserInputMode::PASSTHROUGH},
    {"AUTO", UserInputMode::AUTO},
    {"QUITTING", UserInputMode::QUITTING},
}, "UNKNOWN", UserInputMode::UNKNOWN);

Enum<Session::LineStatus> Session::LineStatusNames({
    {"EMPTY", LineStatus::EMPTY},
    {"INPROCESS", LineStatus::INPROCESS},
    {"LOADED", LineStatus::LOADED},
    {"RELOAD", LineStatus::RELOAD},
}, "UNKNOWN", LineStatus::UNKNOWN);

Enum<Session::OutputMode> Session::OutputModeNames({
    {"ALL", OutputMode::ALL},
    {"NONE", OutputMode::NONE},
    {"FILTERED", OutputMode::FILTERED},
}, "UNKNOWN", OutputMode::UNKNOWN);

Enum<Session::AutoPilotMode> Session::AutoPilotModeNames({
    {"SEMI", AutoPilotMode::SEMI},
    {"FULL", AutoPilotMode::FULL},
}, "UNKNOWN", AutoPilotMode::UNKNOWN);


Session::Session()
: _commandFns{
    // These command lambdas all use `[&]` to capture the `this` pointer.

    {CMD_NOTE, [&] (const Command& cmd) {
        // deliberately empty
    }},

    {CMD_SKIP, [&] (const Command& cmd) {
        _skipping = true;
    }},

    {CMD_RESUME, [&] (const Command& cmd) {
        _skipping = false;
    }},

    {CMD_SET_MODE, [&] (const Command& cmd) {
        // FIXME: use UserInputModeNames
        if (cmd.arg == MODE_PASSTHROUGH) {
            _input_mode = UserInputMode::PASSTHROUGH;

        } else if (cmd.arg == MODE_INSERT) {
            _input_mode = UserInputMode::INSERT;

        } else if (cmd.arg == MODE_AUTO) {
            _input_mode = UserInputMode::AUTO;

        } else if (cmd.arg == MODE_COMMAND) {
            _input_mode = UserInputMode::COMMAND;

        } else {
            // FIXME: make this impossible
            std::cerr << std::endl;
            std::cerr << "Error: unknown " << CMD_OUTPUT << " option: " << cmd.arg << std::endl;
            _quit();

        }
    }},

    {CMD_PAUSE, [&] (const Command& cmd) {
        std::this_thread::sleep_for(std::chrono::milliseconds(boost::lexical_cast<int>(cmd.arg)));
    }},

    {CMD_OUTPUT, [&] (const Command& cmd) {
        // FIXME: use OutputModeNames
        if (cmd.arg == OUTPUT_ALL) {
            _output_mode = OutputMode::ALL;

        } else if (cmd.arg == OUTPUT_NONE) {
            _output_mode = OutputMode::NONE;

        } else {
            // FIXME: make this impossible
            std::cerr << std::endl;
            std::cerr << "Error: unknown " << CMD_OUTPUT << " option: " << cmd.arg << std::endl;
            _quit();

        }
    }},

    {CMD_EXIT, [&] (const Command& cmd) {
        _quit();
    }},

    {CMD_RUN, [&] (const Command& cmd) {
        std::string out = ".gupty-run.out";
        std::string err = ".gupty-run.err";
        // FIXME: append
        boost::process::system(cmd.arg.c_str(), boost::process::std_out > out, boost::process::std_err > err);
    }},

    {CMD_WAIT_FOR_ANY_KEY, [&] (const Command& cmd) {
        _line_status = LineStatus::EMPTY;
        _line = "";
        _line_character_it = _line.begin();
        _process_user_input();
    }},

    {CMD_PASTE_KEYS, [&] (const Command& cmd) {
        // https://stackoverflow.com/questions/236129/how-do-i-iterate-over-the-words-of-a-string/237280#237280
        std::istringstream iss(cmd.arg);
        std::vector<std::string> keys{std::istream_iterator<std::string>{iss}, std::istream_iterator<std::string>{}};
        for (const auto& key : keys) {
            if (keyCodes.find(key) != keyCodes.end()) {
                _send_to_pty(keyCodes.at(key));
            } else {
                // FIXME: make this impossible
                // unknown key - just ignore
            }
        }
    }},

    {CMD_TYPE_KEYS, [&] (const Command& cmd) {
        // https://stackoverflow.com/questions/236129/how-do-i-iterate-over-the-words-of-a-string/237280#237280
        std::istringstream iss(cmd.arg);
        std::vector<std::string> keys{std::istream_iterator<std::string>{iss}, std::istream_iterator<std::string>{}};
        for (const auto& key : keys) {
            if (keyCodes.find(key) != keyCodes.end()) {
                _line_status = LineStatus::EMPTY;
                _line = keyCodes.at(key);
                _line_character_it = _line.begin();
                _process_user_input(false);
                _send_to_pty(keyCodes.at(key));
            } else {
                // FIXME: make this impossible
                // unknown key - just ignore
            }
        }
    }},

    {CMD_WAIT_FOR_AND_SEND_ENTER, [&] (const Command& cmd) {
        _commandFns[CMD_WAIT_FOR_ENTER]({CMD_WAIT_FOR_ENTER, ""}); _commandFns[CMD_PASTE_KEY]({CMD_PASTE_KEY, KEY_Enter});
    }},

    {CMD_WAIT_FOR_ENTER, [&] (const Command& cmd) {
        _line = "";
        _line_character_it = _line.begin();
        _line_status = LineStatus::LOADED;
        _process_user_input();
    }},

    {CMD_PASTE, [&] (const Command& cmd) {
        // if you want to wait for enter after this, then call wait_for_and_send_enter afterwards.
        _send_to_pty(cmd.arg);
    }},

    {CMD_PASTE_LINE, [&] (const Command& cmd) {
        // same as paste, but also send the Enter at the end.
        _commandFns[CMD_PASTE](cmd);
        _commandFns[CMD_PASTE_KEYS]({CMD_PASTE_KEYS, KEY_Enter});
    }},

    {CMD_TYPE_LINE, [&] (const Command& cmd) {
        // send line to shell
        _line_status = LineStatus::EMPTY;
        _line = cmd.arg;
        _line_character_it = _line.begin();
        while (_line_status != LineStatus::LOADED) {  // loop until the line has been marked as loaded

            while (_line_character_it != _line.end()) {  // loop until the last character has been loaded

                // process user input
                _process_user_input();
                if (_line_status == LineStatus::RELOAD) {
                    break;  // exit without processing char
                }

                // need to get char(s) to load AFTER user input because
                // the user might change the iterator
                int n = 1;
                // check if the next characters are part of a multi-character
                // key that should all be sent at the same time.
                auto match_n = multi_char_keys_match(_line_character_it, _line.end());
                if (match_n > 0) {
                    n = match_n;
                }

                _send_to_pty(std::string(_line_character_it, _line_character_it + n));
                _line_character_it += n;

                _line_status = LineStatus::INPROCESS;
            }
            if (_line_status == LineStatus::RELOAD) {
                break;  // go back to top and start over
            }

            _line_status = LineStatus::LOADED;

            // process user input.
            // the user may press backspace, in which case
            // the line status will be reset to processing
            // and we will need to go back to the top.
            _process_user_input();
            if (_line_status == LineStatus::LOADED) {
                _send_to_pty(CODE_Enter);
            }
        }
    }},

    {CMD_TYPE, [&] (const Command& cmd) {
        // same as type_line, but without requiring the Enter after the line is done
        // (eg. so you can do more line editing with type_keys)

        // send line to shell
        _line_status       = LineStatus::EMPTY;
        _line = cmd.arg;
        _line_character_it = _line.begin();
        while (_line_status != LineStatus::LOADED) {  // loop until the line has been marked as loaded

            while (_line_character_it != _line.end()) {  // loop until the last character has been loaded

                // process user input
                _process_user_input();
                if (_line_status == LineStatus::RELOAD) {
                    break;  // exit without processing char
                }

                // need to get char(s) to load AFTER user input because
                // the user might change the iterator
                int n = 1;
                // check if the next characters are part of a multi-character
                // key that should all be sent at the same time.
                auto match_n = multi_char_keys_match(_line_character_it, _line.end());
                if (match_n > 0) {
                    n = match_n;
                }

                _send_to_pty(std::string(_line_character_it, _line_character_it + n));
                _line_character_it += n;

                _line_status = LineStatus::INPROCESS;
            }
            if (_line_status == LineStatus::RELOAD) {
                break;  // go back to top and start over
            }
            _line_status = LineStatus::LOADED;
        }
    }},
}
{
    // aliases
    // FIXME: make a lambda generator for aliases, and use it to define these aliases above next to their actual command
    _commandFns[CMD_PASTE_KEY] = _commandFns[CMD_PASTE_KEYS];
    _commandFns[CMD_TYPE_KEY] = _commandFns[CMD_TYPE_KEYS];
}

void Session::setShell(const std::string& shell) {
    _shell = shell;
    if (_shell == "") {
        if (auto env_shell = getenv("SHELL")) {
            _shell = env_shell;
        } else {
            _shell = "sh";
        }
    }
}

void Session::setMonitor(const std::string& monitor_filename) {
    _monitor_filename = monitor_filename;
}

void Session::setNoMonitor() {
    _monitor_filename.reset();
}


void Session::init() {
    if (_monitor_filename) {
        _monitor_file = new std::ofstream(*_monitor_filename, std::ios::binary | std::ios::out | std::ios::trunc);
        runtime_assert(_monitor_file != nullptr, "Could not open monitor file.");
    }

    runtime_assert(tcgetattr(STDIN_FILENO, &_orig_terminal_settings) == 0, "Could not retrieve terminal settings on stdin.");

    _pty_fd = posix_openpt(O_RDWR);
    runtime_assert(_pty_fd >= 0, "There was a problem opening pty.");

    BOOST_LOG_TRIVIAL(debug) << "Opened pseudoterminal.";
    BOOST_LOG_TRIVIAL(debug) << "  Pty device fd: " << _pty_fd;

    runtime_assert(grantpt(_pty_fd) == 0, "Could not grant access to pty.");
    runtime_assert(unlockpt(_pty_fd) == 0, "Could not unlock pty device.");

    _pty_device_name = ptsname(_pty_fd);
    runtime_assert(_pty_device_name != nullptr, "Could not get pty device name.");
    BOOST_LOG_TRIVIAL(debug) << "  Pty device name: " << _pty_device_name;

    _child_pid = fork();
    runtime_assert(_child_pid >= 0, "Could not fork child process.");

    if (_child_pid == 0) {
        // In the child, set up and run the shell.
        BOOST_LOG_TRIVIAL(debug) << "Closing pty fd.";
        runtime_assert(close(_pty_fd) == 0, "Unable to close pty fd.");
        _pty_fd = -2;

        BOOST_LOG_TRIVIAL(debug) << "Creating new session for child.";
        runtime_assert(setsid() != -1, "Could not start new session.");

        BOOST_LOG_TRIVIAL(debug) << "Opening pty device in child to act as controlling terminal.";
        auto fd = open(_pty_device_name, O_RDWR);
        runtime_assert(fd >= 0, "Could not open pty device file.");

        BOOST_LOG_TRIVIAL(debug) << "Connecting child stdin, stdout, and stderr to pty device.";
        runtime_assert(dup2(fd, 0) == 0, "Could not connect child stdin to pty device");
        runtime_assert(dup2(fd, 1) == 1, "Could not connect child stdout to pty device");
        runtime_assert(dup2(fd, 2) == 2, "Could not connect child stderr to pty device");

        BOOST_LOG_TRIVIAL(debug) << "Closing fd.";
        runtime_assert(close(fd) == 0, "Unable to close fd.");

        std::vector<char*> argv;
        argv.push_back(raw_c_str(_shell));
        std::transform(_shell_args.begin(), _shell_args.end(), std::back_inserter(argv), raw_c_str);
        argv.push_back(NULL);  // need to null terminate the array

        BOOST_LOG_TRIVIAL(debug)
            << "Launching shell (" << _shell << ") in child process with "
            << argv.size() << " element array for argv:";
        for (const auto& a : argv) {
            if (a != NULL) {
                BOOST_LOG_TRIVIAL(debug) << "  " << a;
            } else {
                BOOST_LOG_TRIVIAL(debug) << "  NULL";
            }
        }

        execvp(_shell.c_str(), argv.data());
        runtime_assert(false, "execvp failed");  // FIXME include strerror(errno)
    }

    // set terminal to raw mode
    termios terminal_settings = _orig_terminal_settings;
    cfmakeraw(&terminal_settings);
    runtime_assert(tcsetattr(STDIN_FILENO, TCSANOW, &terminal_settings) == 0, "Could not set terminal settings on stdin.");

    // set the pty window size to match parents
    _sync_window_size();

    _inited = true;
}

Session::~Session() try {
    if ( ! _inited) {
        return;
    }

    BOOST_LOG_TRIVIAL(debug) << "Session::~Session starting";
    _input_mode = UserInputMode::QUITTING;
    _updateMonitor();

    runtime_assert(close(_pty_fd) == 0, "Unable to close _pty_fd.");
    runtime_assert(tcsetattr(STDIN_FILENO, TCSANOW, &_orig_terminal_settings) == 0, "Could not reset terminal settings on stdin.");

    BOOST_LOG_TRIVIAL(debug) << "killing child process";
    // No need to check for failure - if the process has already gone away, then good.
    // The other failure modes (EINVAL (invalid signal), EPERM (no perms to send signal)) are impossible here.
    kill(_child_pid, SIGKILL);

    BOOST_LOG_TRIVIAL(debug) << "Session::~Session finished";
} catch (const std::runtime_error& e) {
    std::cerr << "Error: Session dtor: " << e.what() << std::endl;
    BOOST_LOG_TRIVIAL(error) << e.what();
}

Commands Session::resolveCommands(const Lines& lines) {
    Commands commands;
    for (const auto& line : lines) {
        if (line[0] == '\0' || line[0] == '#') {
            continue;
        }
        auto spacepos = line.find(" ");  // if none, will return npos
        auto name = line.substr(0, spacepos);
        if (_commandFns.find(name) == _commandFns.end()) {
            // unknown command
            std::cerr << "Error: unknown command: " << name << std::endl;
            throw std::runtime_error("unknown command");
        }
        auto arg = (spacepos != std::string::npos) ? line.substr(spacepos + 1) : "";
        if (name == "include") {
            auto subcmds = resolveCommands(readLines(arg));
            commands.insert(commands.end(), std::make_move_iterator(subcmds.begin()), std::make_move_iterator(subcmds.end()));
        } else {
            commands.push_back({name, arg});
        }
    }
    return commands;
}

constexpr auto CODE_clearscr = "\033[3J\033[H\033[2J";

constexpr auto FMT_RESET = "\033[0m";
constexpr auto FMT_BOLD = "\033[1m";
constexpr auto FMT_FAINT = "\033[2m";
constexpr auto FMT_INVERSE = "\033[3m";

constexpr auto FMT_FG_BLACK = "\033[30m";
constexpr auto FMT_FG_RED = "\033[31m";
constexpr auto FMT_FG_GREEN = "\033[32m";
constexpr auto FMT_FG_YELLOW = "\033[33m";
constexpr auto FMT_FG_BLUE = "\033[34m";
constexpr auto FMT_FG_MAGENTA = "\033[35m";
constexpr auto FMT_FG_CYAN = "\033[36m";
constexpr auto FMT_FG_WHITE = "\033[37m";

constexpr auto FMT_BG_BLACK = "\033[40m";
constexpr auto FMT_BG_RED = "\033[41m";
constexpr auto FMT_BG_GREEN = "\033[42m";
constexpr auto FMT_BG_YELLOW = "\033[43m";
constexpr auto FMT_BG_BLUE = "\033[44m";
constexpr auto FMT_BG_MAGENTA = "\033[45m";
constexpr auto FMT_BG_CYAN = "\033[46m";
constexpr auto FMT_BG_WHITE = "\033[47m";

constexpr auto FMT_FG_BRIGHT_BLACK = "\033[90m";
constexpr auto FMT_FG_BRIGHT_RED = "\033[91m";
constexpr auto FMT_FG_BRIGHT_GREEN = "\033[92m";
constexpr auto FMT_FG_BRIGHT_YELLOW = "\033[93m";
constexpr auto FMT_FG_BRIGHT_BLUE = "\033[94m";
constexpr auto FMT_FG_BRIGHT_MAGENTA = "\033[95m";
constexpr auto FMT_FG_BRIGHT_CYAN = "\033[96m";
constexpr auto FMT_FG_BRIGHT_WHITE = "\033[97m";

constexpr auto FMT_BG_BRIGHT_BLACK = "\033[100m";
constexpr auto FMT_BG_BRIGHT_RED = "\033[101m";
constexpr auto FMT_BG_BRIGHT_GREEN = "\033[102m";
constexpr auto FMT_BG_BRIGHT_YELLOW = "\033[103m";
constexpr auto FMT_BG_BRIGHT_BLUE = "\033[104m";
constexpr auto FMT_BG_BRIGHT_MAGENTA = "\033[105m";
constexpr auto FMT_BG_BRIGHT_CYAN = "\033[106m";
constexpr auto FMT_BG_BRIGHT_WHITE = "\033[107m";

void Session::_updateMonitor() {
    if (_monitor_file == nullptr) {
        return;
    }
    *_monitor_file << CODE_clearscr;


    std::string fmt_statusline;

    // FIXME: use a lookup table
    if (_input_mode == UserInputMode::QUITTING) {
        fmt_statusline += FMT_BG_RED;
        fmt_statusline += FMT_FG_WHITE;
    } else if (_input_mode == UserInputMode::INSERT) {
        fmt_statusline += FMT_BG_BRIGHT_GREEN;
        fmt_statusline += FMT_FG_BLACK;
    } else if (_input_mode == UserInputMode::COMMAND) {
        fmt_statusline += FMT_BG_BRIGHT_YELLOW;
        fmt_statusline += FMT_FG_BLACK;
    } else if (_input_mode == UserInputMode::PASSTHROUGH) {
        fmt_statusline += FMT_BG_BRIGHT_BLUE;
        fmt_statusline += FMT_FG_WHITE;
    } else if (_input_mode == UserInputMode::AUTO) {
        fmt_statusline += FMT_RESET;
    }
    *_monitor_file << fmt_statusline << "Input mode: " << FMT_BOLD << UserInputModeNames(_input_mode) << FMT_RESET << std::endl;
    *_monitor_file << std::endl;


    auto total_lines = _commands.size();
    auto num_digits  = boost::lexical_cast<std::string>(total_lines).length();
    auto it = _current_command;
    if (_monitor_num_pre_lines + _monitor_num_total_lines > total_lines) {
        // just always show the full thing
        it = _commands.begin();
    } else if (_commands.end() - it < _monitor_num_total_lines - _monitor_num_pre_lines) {
        it = _commands.end() - _monitor_num_total_lines;
    } else {
        for (unsigned int i = 0; i < _monitor_num_pre_lines && it != _commands.begin(); i++) {
            it--;
        }
    }

    for (unsigned int i = 0; i < _monitor_num_total_lines && it != _commands.end(); i++) {
        auto origw = _monitor_file->width();
        std::string fmt_name = FMT_FG_GREEN;
        std::string fmt_arg = FMT_BOLD;
        if (it->name == CMD_NOTE) {
            fmt_arg += FMT_FG_CYAN;
        }
        *_monitor_file << (it == _current_command ? " --> " : "     ") << std::setw(num_digits) << (it - _commands.begin() + 1) << std::setw(origw) << ": " << fmt_name << it->name << FMT_RESET << " " << fmt_arg << it->arg << FMT_RESET << std::endl;
        it++;
    }

    *_monitor_file << std::endl;
    *_monitor_file << "Total lines: " << total_lines  << std::endl;
}

void Session::run(Commands commands) {
    BOOST_LOG_TRIVIAL(debug) << "Beginning session run.";

    _commands = commands;
    _current_command = _commands.begin();

    // process script and user input
    while (_current_command != _commands.end()) {
        if (_skipping) {
            _current_command++;
            if (_current_command->name == CMD_RESUME) {
                _commandFns[_current_command->name](*_current_command);
            }
            continue;
        }

        if (_commandFns.find(_current_command->name) != _commandFns.end()) {
            _updateMonitor();
            _commandFns[_current_command->name](*_current_command);
            _updateMonitor();
        } else {
            // unknown command - should not be possible
            std::cerr << std::endl;
            std::cerr << "Error: unknown command: " << _current_command->name << std::endl;
            _quit();
        }

        if (_line_status != LineStatus::RELOAD) {
            _current_command++;  // don't advance line pointer if we need to reload
        }
    }

    // out of commands - go into free typing (passthrough) mode
    if (_input_mode != UserInputMode::AUTO) {
        _commandFns[CMD_SET_MODE]({CMD_SET_MODE, MODE_PASSTHROUGH});
        // if the user exits passthrough mode, goes into insert mode, and then presses enter, then we will exit.
        // otherwise, the user can just exit passthrough mode into command mode, and type q to exit.
        _commandFns[CMD_WAIT_FOR_ENTER]({CMD_WAIT_FOR_ENTER, ""});
    }

    BOOST_LOG_TRIVIAL(debug) << "Session run completed.";
}

void Session::_process_pty_output() {
    // check if the pty has outputted anything, and if so, read it and
    // deal with it.
    char ch;
    int  rc;

    pollfd polls;
    polls.fd = _pty_fd;
    polls.events = POLLIN;

    while (true) {
        rc = poll(&polls, 1, 0);
        if (rc < 0) {
            throw std::runtime_error("There was a problem polling pty fd.");
        } else if (rc == 0) {
            break;
        }
        _send_to_stdout(_get_from_pty());
    }
}

namespace {

std::string read_from_fd(int fd) {
    constexpr size_t BUF_SIZE = 128;
    char buffer[BUF_SIZE];
    std::string s;

    while (s.empty()) {

        auto count = read(fd, buffer, BUF_SIZE);
        s += std::string(buffer, count);

        while (true) {
            pollfd stdinpoll;
            stdinpoll.fd = fd;
            stdinpoll.events = POLLIN;
            int rc = poll(&stdinpoll, 1, 0);
            if (rc < 0) {
                throw std::runtime_error("There was a problem polling stdin.");
            } else if (rc == 0) {
                break;
            }
            auto count = read(fd, buffer, BUF_SIZE);
            s += std::string(buffer, count);
        }
    }

    return s;
}

void write_to_fd(int fd, const std::string& s) {
    const auto cstr = s.c_str();
    const auto len = s.length();
    auto num_written = 0;
    num_written += write(fd, cstr + num_written, len - num_written);
    while (num_written < len) {
        num_written += write(fd, cstr + num_written, len - num_written);
    }
}

}

void Session::_read_from_stdin() {
    std::string s = read_from_fd(STDIN_FILENO);

    // there's nothing left in stdin, and s is not empty, so we can process s now
    while ( ! s.empty()) {
        auto match_n = multi_char_keys_match(s);
        if (match_n == 0) {
            // unrecognised.  so just peel off 1 char.
            match_n = 1;
        }
        _pendingKeys.push_back(s.substr(0, match_n));
        s = s.substr(match_n);
    }
}

std::string Session::_get_key_from_stdin() {

    _updateMonitor();

    while (true) {

        // instead of a blocking read on stdin, this is a
        // blocking poll on stdin + _pty_fd.  then, when it returns,
        // react according, ie. handle either or both which have input to read.
        //
        // if there was something from stdin, then proceed as we do here,
        // ie. go ahead and use a non-blocking poll to make sure we consume
        // everything available for stdin, then chop it up and put it into
        // _pendingKeys.
        //
        // if there was something from the pty, then handle it accordingly,
        // ie. read it and then send it to stdout.
        //
        // finally, after doing either/both of these things, check if _pendingKeys
        // has anything in it, and if so, return the first thing.

        pollfd polls[2];
        polls[0].fd = STDIN_FILENO;
        polls[0].events = POLLIN;
        polls[0].revents = 0;
        polls[1].fd = _pty_fd;
        polls[1].events = POLLIN;
        polls[1].revents = 0;
        // timeout should only be -1 if _pendingKeys.size() == 0.
        // Otherwise, it should be 0 - this lets us still handle any pty output
        // (or any extra stdin for that matter), and then fall immediately through to
        // return the pendingkey.
        int timeout = (_pendingKeys.size() == 0) ? -1 : 0;
        int rc = poll(polls, 2, timeout);
        if (rc < 0) {
            throw std::runtime_error("There was a problem polling stdin.");
        } else if (rc > 0) {
            // Something happened
            if (polls[0].revents & POLLERR) {
                throw std::runtime_error("Error encountered while polling stdin.");
            }
            if (polls[1].revents & POLLERR) {
                throw std::runtime_error("Error encountered while polling pty.");
            }
            if (polls[1].revents & POLLIN) {
                // There is data to read from the pty.
                _process_pty_output();
            }
            if (polls[0].revents & POLLIN) {
                // There is data to read from stdin.
                _read_from_stdin();
            }
        }

        if (_pendingKeys.size() > 0) {
            auto key = _pendingKeys.front();
            _pendingKeys.pop_front();
            return key;
        }
    }
}

void Session::_send_to_stdout(const std::string& s) {
    if (_output_mode == OutputMode::ALL) {
        write_to_fd(STDOUT_FILENO, s);

    } else if (_output_mode == OutputMode::NONE) {

    } else  {
        // FIXME: handle output filtering...?
    }
}

std::string Session::_get_from_pty() {
    // output of pty is read from pty fd
    return read_from_fd(_pty_fd);
}

// Since we need to mutate the string (to change \n to \r),
// we may as well just pass it in by value anyway.
void Session::_send_to_pty(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [] (const char& ch) {
        return ch == '\n' ? '\r' : ch;
    });
    // It's possible that some programs might not like getting lots of
    // "typed input" all at once.  So it might be good to have an option
    // to specify some delay between each key sent to the pty (even when
    // "pasting").
    write_to_fd(_pty_fd, s);
}

void Session::_process_user_input(bool permit_backspace) {
    bool cont;

    cont = true;
    while (cont) {
        // by default, we loop once.
        cont = false;

        if (_input_mode == UserInputMode::COMMAND) {
            // read input and process as commands
            while (true) {
                auto key = _get_key_from_stdin();
                auto action = _command_keys.get(key);

                if (action == Mode::Command::Actions::SigInt) {
                    // if the user presses Ctl-C, we need to send SIGINT to everybody in
                    // our process group (ie. kill any sub-processes).
                    kill(0, SIGINT);
                    throw exception::early_exit();

                } else if (action == Mode::Command::Actions::SigQuit) {
                    // Ctl-\, which means quit
                    kill(0, SIGQUIT);
                    throw exception::early_exit();

                } else if (action == Mode::Command::Actions::Quit) {
                    _quit_early();

                } else if (action == Mode::Command::Actions::ResizeWindow) {
                    _sync_window_size();

                } else if (action == Mode::Command::Actions::SwitchToInsertMode) {
                    _input_mode = UserInputMode::INSERT;
                    cont = true;
                    break;

                } else if (action == Mode::Command::Actions::SwitchToPassthroughMode) {
                    _input_mode = UserInputMode::PASSTHROUGH;
                    cont = true;
                    break;

                } else if (action == Mode::Command::Actions::SwitchToAutoMode) {
                    _input_mode = UserInputMode::AUTO;
                    cont = true;
                    break;

                } else if (action == Mode::Command::Actions::TurnOffStdout) {
                    _output_mode = OutputMode::NONE;

                } else if (action == Mode::Command::Actions::TurnOnStdout) {
                    _output_mode = OutputMode::ALL;

                } else if (action == Mode::Command::Actions::ToggleStdout) {
                    if (_output_mode == OutputMode::NONE) {
                        _output_mode = OutputMode::ALL;
                    } else if (_output_mode == OutputMode::ALL) {
                        _output_mode = OutputMode::NONE;
                    }

                } else if (action == Mode::Command::Actions::NextLine) {
                    // FIXME: implement?

                } else if (action == Mode::Command::Actions::PrevLine) {
                    // FIXME: implement?

                } else if (action == Mode::Command::Actions::Return) {
                    break;
                }
                // Unrecognized keys (ie. no associated action) are ignored,
                // and we just keep getting more input.
            }


        } else if (_input_mode == UserInputMode::INSERT) {
            while (true) {
                auto key = _get_key_from_stdin();
                auto action = _insert_keys.get(key);

                if (_line == "") {
                    permit_backspace = false;
                }

                if (action == Mode::Insert::Actions::SigInt) {
                    // if the user presses Ctl-C, we need to send SIGINT to everybody in
                    // our process group (ie. kill any sub-processes).
                    kill(0, SIGINT);
                    throw exception::early_exit();

                } else if (action == Mode::Insert::Actions::SigQuit) {
                    // Ctl-\, which means quit
                    kill(0, SIGQUIT);
                    throw exception::early_exit();

                } else if (action == Mode::Insert::Actions::BackOneCharacter) {
                    if ( ! permit_backspace) {
                        // if backspace isn't permitted, then treat it the same as Disabled/ignored (ie. continue).
                        // except for if the character that is going to be emitted by insert mode, is also a backspace
                        // character!  in this case, it should be actually emitted (since the user typed it), so then
                        // we treat it the same as when typing any other random character (ie. break).
                        if (*_line_character_it == CODE_Backspace[0]) { // This hack works because this code is just 1 char
                            break;
                        } else {
                            continue;
                        }
                    }
                    // need to: send backspace to shell, rewind the line character
                    // iterator, and start over
                    LineStatus init_line_status = _line_status;
                    if (_line_character_it > _line.begin()) {
                        // only delete characters if at least one is loaded.
                        _send_to_pty(CODE_Backspace);
                        _line_character_it--;
                        _line_status = LineStatus::INPROCESS;
                    }
                    if (_line_character_it == _line.begin()) {
                        _line_status = LineStatus::EMPTY;
                    }
                    if (init_line_status != LineStatus::LOADED) {
                        // if line was loaded, we need to break immediately so
                        // that the input loop will be restarted
                        cont = true;
                    }
                    break;

                } else if (action == Mode::Insert::Actions::SwitchToCommandMode) {
                    _input_mode = UserInputMode::COMMAND;
                    // Start the input loop over again (in command mode this time).
                    cont = true;
                    break;

                } else if (action == Mode::Insert::Actions::Return) {
                    break;

                } else if (action == Mode::Insert::Actions::Disabled) {
                    continue;

                } else if (_line_status == LineStatus::LOADED) {
                    // if a line has been loaded, don't return unless
                    // the user presses Enter (which is handled above)
                    continue;

                } else {
                    // return back to caller for any key that isn't handled
                    // specifically above.
                    break;
                }
            }


        } else if (_input_mode == UserInputMode::PASSTHROUGH) {
            while (true) {
                auto key = _get_key_from_stdin();
                auto action = _passthrough_keys.get(key);

                if (action == Mode::Passthrough::Actions::SwitchToCommandMode) {
                    _input_mode = UserInputMode::COMMAND;
                    cont = true;
                    break;

                } else {
                    _send_to_pty(key);
                }
            }


        } else if (_input_mode == UserInputMode::AUTO) {
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(STDIN_FILENO, &fds);

            timeval timeout;
            timeout.tv_sec  = 0;
            timeout.tv_usec = 0;

            int rc;

            rc = select(1, &fds, NULL, NULL, &timeout);
            if (rc < 0) {
                throw std::runtime_error("There was a problem polling stdin fd.");
            }

            // if we are in semi-auto mode and a line has been
            // loaded, then we need to wait for user input
            if (_auto_pilot_mode == AutoPilotMode::SEMI && _line_status == LineStatus::LOADED) {
                cont = true;
            }

            if (rc > 0) {
                // Since this is auto mode, only read from stdin if there is actually anything there to read.
                // Otherwise we'll block.

                auto key = _get_key_from_stdin();
                auto action = _auto_keys.get(key);

                if (action == Mode::Auto::Actions::SigInt) {
                    // if the user presses Ctl-C, we need to send SIGINT to everybody in
                    // our process group
                    kill(0, SIGINT);
                    throw exception::early_exit();

                } else if (action == Mode::Auto::Actions::SigQuit) {
                    // Ctl-\, which means quit
                    kill(0, SIGQUIT);
                    throw exception::early_exit();

                } else if (action == Mode::Auto::Actions::SwitchToCommandMode) {
                    _input_mode = UserInputMode::COMMAND;

                } else if (action == Mode::Auto::Actions::SwitchToFullAuto) {
                    _auto_pilot_mode = AutoPilotMode::FULL;

                } else if (action == Mode::Auto::Actions::SwitchToSemiAuto) {
                    _auto_pilot_mode = AutoPilotMode::SEMI;

                } else if (action == Mode::Auto::Actions::Return) {
                    break;
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(_auto_pilot_pause_milliseconds));
        }
    }
}

void Session::_sync_window_size() {
    winsize window_size;
    runtime_assert(ioctl(STDIN_FILENO, TIOCGWINSZ, &window_size) == 0, "Could not get current window size");
    BOOST_LOG_TRIVIAL(debug) << "got window_size of rows = " << window_size.ws_row << " cols = " << window_size.ws_col << " xpixel = " << window_size.ws_xpixel << " ypixel = " << window_size.ws_ypixel;
    runtime_assert(window_size.ws_row != 0, "window size rows is zero");
    runtime_assert(window_size.ws_col != 0, "window size cols is zero");
    window_size.ws_xpixel = 0;
    window_size.ws_ypixel = 0;
    auto result = ioctl(_pty_fd, TIOCSWINSZ, &window_size);
    auto set_errno = errno;
    BOOST_LOG_TRIVIAL(debug) << "set window size result = " << result << " and errno " << set_errno << " " << strerror(set_errno);
    //runtime_assert(result == 0, "Could not set pty window size");  // fails on macos, not sure why
}

void Session::_quit(bool early) {
    BOOST_LOG_TRIVIAL(debug) << "_quit() called.";
    _input_mode = UserInputMode::QUITTING;
    _updateMonitor();
    if (early) {
        throw exception::early_exit();
    } else {
        throw exception::normal_exit();
    }
}

void Session::_quit_early() {
    _quit(true);
}
