`gupty` - guided pseudo terminal
================================

**Live terminal demos that are just as smooth as pre-recorded**

![Gupty demo](./demo/readme-demo.gif)

[[larger](./demo/readme-demo.gif)] [[mp4](./demo/readme-demo.mp4)] [[youtube](https://www.youtube.com/watch?v=9dgrDjldbPA)] [[source](./demo/readme-demo.gupty)]

As you type, `gupty` feeds your scripted input to the terminal, rather than the characters you're typing.  This means you don't have to worry about making typos.  However, the demo is still live, and you can drop to a passthrough mode if you want to go off-script (eg. if someone asks an unexpected question), or need to fix something unexpectedly.  There's also a way to monitor where you're up to in the demo, so that you know what's coming up and aren't typing blindly.

Supports Linux and MacOS.

Inspired by and originally based on [gsc](https://github.com/CD3/gsc), but completely rewritten with fewer bugs, improved syntax, and simpler monitoring.


Installation
------------

Either download binaries from the [Releases](https://github.com/mongodb-labs/gupty/releases) page, or compile from source (see below).


Compilation
-----------

Requirements:

- cmake 3.10 or later
- boost (`log`, `program_options`, `container`)

```
git clone https://github.com/mongodb-labs/gupty
cd gupty
cmake -S . -B build
cmake --build build
sudo cmake --build build -t install
```

To install to a particular prefix, add `-DCMAKE_INSTALL_PREFIX=<target_location>` to the first `cmake` invocation.


Running
-------

In the terminal that will be shown to the audience, run:

```
gupty <input_file.gupty>
```

Then, in a terminal that only you can see (but in the same working directory as the above command):

```
tail -f -n +0 .gupty.monitor
```


Usage
-----

gupty is modal.

- `INSERT` mode (default):
    - `<ctrl-c>` - exit gupty
    - `<ctrl-\>` - exit gupty
    - `<esc>` - go to `COMMAND` mode
    - `<backspace>` - undo the previously typed key (if possible)
    - `<enter>` - confirm the line (if waiting for Enter)
    - any other key - simulates typing

- `COMMAND` mode:
    - `<ctrl-c>` - exit gupty
    - `<ctrl-\>` - exit gupty
    - `q` - exit gupty
    - `i` - go to `INSERT` mode
    - `p` - go to `PASSTHROUGH` mode
    - `r` - make gupty notice a change in window size (may not work on MacOS)

- `PASSTHROUGH` mode:
    - `<ctrl-d>` - go to `COMMAND` mode
    - any other key - passed through to the underlying terminal

- `AUTO` mode (untested/unsupported):
    - `<ctrl-c>` - exit gupty
    - `<ctrl-\>` - exit gupty
    - `<esc>` - go to `COMMAND` mode
    - `f` - switch to full auto
    - `s` - switch to semi auto
    - `<enter>` - confirm the line (if waiting for Enter)


Commands
--------

These commands can be used in the `input_file.gupty` file.  Blank lines are ignored.  Lines where the first character is `#` are completely ignored (ie. are comments), and will not be parsed at all (ie. will not show up in the monitor file, use `note` for that).

- `note <comments...>` - The remainder of the line is ignored (ie. this is a comment).  Sometimes more useful than `#` because it will appear in the `.gupty.monitor` file.
- `skip` - Start skipping commands.  Subsequent commands (which must still be valid) will not be processed, until the `resume` command is encountered.
- `resume` - Stop skipping commands.
- `set_mode <insert|command|passthrough|auto>` - Enter the given mode.
- `pause <millis>` - Wait for the given number of milliseconds.  Currently, output from the underlying terminal is not processed during this time, so try to keep these times as short as possible.
- `output none` - Output from the underlying terminal is not shown.
- `output all` - Output from the underlying terminal is shown.
- `exit` - Exit gupty.
- `run <cmd> <args...>` - Execute the remainder of the line via system(3). Output is not shown (but is instead send to `.gupty-run.out` and `.gupty-run.err`).

- `wait_for_any_key` - Wait for any key to be pressed.
- `paste_keys <key_name> [<key_name> ...]` - Immediately paste all the listed keys into the underlying terminal.
- `paste_key <key_name> [<key_name> ...]` - Alias for `paste_keys`.
- `type_keys <key_name> [<key_name> ...]` - Type the listed keys one by one, as user input is received, into the underlying terminal.
- `type_key <key_name> [<key_name> ...]` - Alias for `type_keys`.
- `wait_for_and_send_enter` - Wait for Enter to be pressed, and when it is, send it to the underlying terminal.
- `wait_for_enter` - Wait for Enter to be pressed (but when it is, do not send it to the underlying terminal).
- `paste <line>` - Immediately paste the remainder of the line (without a trailing Enter) to the underlying terminal.
- `paste_line <line>` - Immediately paste the remainder of the line (with a trailing Enter) to the underlying terminal (ie. the same as `paste`, but send Enter at the end).
- `type <line>` - Type the characters of the remainder of the line, one by one, as user input is received, into the underlying terminal.  This command ends as soon as the last character has been sent (eg. if you then want to do more line editing with `type_keys`).
- `type_line <line>` - Type the characters of the remainder of the line, one by one, as user input is received, into the underlying terminal, waiting for Enter to be pressed at the end.


Key names
---------

These can be used in place of `<key_name>` above.

- `Enter`
- `Return`
- `Backspace`
- `Up`
- `Down`
- `Left`
- `Right`
- `Insert`
- `Delete`
- `Home`
- `End`
- `PageUp`
- `PageDown`

