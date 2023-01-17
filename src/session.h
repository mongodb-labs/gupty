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

#pragma once

#include <string>
#include <vector>
#include <list>
#include <map>
#include <optional>
#include <ostream>

#include <termios.h>

#include "libgupty.h"
#include "lines.h"
#include "mode_auto.h"
#include "mode_command.h"
#include "mode_insert.h"
#include "mode_passthrough.h"

struct Command {
    std::string name;
    std::string arg;
};

using Commands = std::vector<Command>;

using CommandFn = std::function<void(const Command&)>;

class Session {
public:
    enum class UserInputMode {
        COMMAND,
        INSERT,
        PASSTHROUGH,
        AUTO,
        QUITTING,
        UNKNOWN,
    };
    static Enum<UserInputMode> UserInputModeNames;

    enum class LineStatus {
        EMPTY,
        INPROCESS,
        LOADED,
        RELOAD,
        UNKNOWN,
    };
    static Enum<LineStatus> LineStatusNames;

    enum class OutputMode {
        ALL,
        NONE,
        FILTERED,
        UNKNOWN,
    };
    static Enum<OutputMode> OutputModeNames;

    enum class AutoPilotMode {
        SEMI,
        FULL,
        UNKNOWN,
    };
    static Enum<AutoPilotMode> AutoPilotModeNames;


    Session();
    ~Session();

    void setShell(const std::string& shell);
    void setMonitor(const std::string& monitor_filename);
    void setNoMonitor();

    void init();
    Commands resolveCommands(const Lines& lines);
    void run(Commands commands);


protected:
    void _updateMonitor();
    void _process_pty_output();

    void _read_from_stdin();
    std::string _get_key_from_stdin();
    void _send_to_stdout(const std::string& s);

    std::string _get_from_pty();
    void _send_to_pty(std::string s);

    void _process_user_input(bool permit_backspace = true);

    void _sync_window_size();

    void _quit(bool early = false);
    void _quit_early();


    UserInputMode _input_mode = UserInputMode::INSERT;
    LineStatus _line_status = LineStatus::EMPTY;
    OutputMode _output_mode = OutputMode::ALL;
    AutoPilotMode _auto_pilot_mode = AutoPilotMode::FULL;

    int _pty_fd = -2;
    char* _pty_device_name = NULL;
    pid_t _child_pid = -2;

    int _auto_pilot_pause_milliseconds = 100;

    bool _skipping = false;

    termios _orig_terminal_settings;

    std::string _line;
    std::string::const_iterator _line_character_it;

    std::string _shell;
    // FIXME: currently unused (but could be)
    std::vector<std::string> _shell_args;

    const Mode::Insert::Keys _insert_keys;
    const Mode::Command::Keys _command_keys;
    const Mode::Passthrough::Keys _passthrough_keys;
    const Mode::Auto::Keys _auto_keys;

    bool _inited = false;

    Commands _commands;
    Commands::iterator _current_command;

    std::map<std::string,CommandFn> _commandFns;

    std::optional<std::string> _monitor_filename;
    std::ostream* _monitor_file = nullptr;
    // FIXME: make this configurable
    unsigned int _monitor_num_pre_lines = 10;
    unsigned int _monitor_num_total_lines = 30;

    std::list<std::string> _pendingKeys;

};

