#!/usr/bin/env python3

import os

import pexpect

from pexpect_helper import SpawnedProc, control

env = os.environ.copy()
env["TERM"] = "xterm-kitty"
env["FISH_TEST_NO_RECURRENT_QUERIES"] = ""
sp = SpawnedProc(dimensions=(6, 30), env=env)
sp.send_cursor_position_report(y=1, x=1)
sp.expect_prompt()
sp.sendline("set -g FISH_TEST_NO_RECURRENT_QUERIES 1")
sp.send_primary_device_attribute()
sp.expect_prompt()
sp.expect_str("\x1b[?1000h", shouldfail=True, timeout=0.1)
sp.sendline("stty size")
sp.expect_prompt("6 30")
sp.sendline("echo $LINES $COLUMNS")
sp.expect_prompt("6 30")


def read_available():
    chunks = []
    while True:
        try:
            chunks.append(sp.spawn.read_nonblocking(65536, timeout=0.02))
        except pexpect.TIMEOUT:
            return "".join(chunks)


# Capture the commandline cursor without modifying the commandline. We inspect it
# after cancelling the pending command. Direct input-function bindings make cursor
# motion deterministic without temporarily handing the terminal away.
sp.sendline(
    "bind ctrl-g 'set -g viewport_cursor (commandline --cursor)'; "
    "bind ctrl-y up-line; bind ctrl-x down-line"
)
sp.expect_prompt()

# Leave some ordinary output in terminal scrollback, then paste a command whose
# visual height is greater than the terminal height without executing it.
sp.sendline("seq 20")
sp.expect_prompt("20")
commandline = "begin\n" + "\n".join(f"# viewport-{i:02d}" for i in range(16))
sp.send("\x1b[200~" + commandline + "\x1b[201~")
initial_status = sp.expect_re(r"rows (\d+) to (\d+) of (\d+)\. Enter")
initial_start, initial_stop, total = map(int, initial_status.groups())

# Fish owns the wheel only while its commandline viewport is scrollable. Normal
# button tracking is sufficient for wheel reports; motion tracking must remain
# disabled.
sp.expect_str("\x1b[?9;1000;1001;1002;1003;1005;1006;1015;1016s")
sp.expect_str("\x1b[?1006h")
sp.expect_str("\x1b[?1000h")
sp.expect_str("\x1b[?1002h", shouldfail=True, timeout=0.1)

# Moving within the visible window changes only the terminal cursor. The fifth line
# move crosses an edge and advances the visual window by exactly one row.
sp.send(control("y") * 4)
sp.sleep(0.1)
movement_output = read_available()
assert "viewport-" not in movement_output
assert "rows " not in movement_output
sp.send(control("y"))
sp.expect_re(rf"rows {initial_start - 1} to {initial_stop - 1} of {total}\. Enter")

sp.send(control("x") * 4)
sp.sleep(0.1)
movement_output = read_available()
assert "viewport-" not in movement_output
assert "rows " not in movement_output
sp.send(control("x"))
sp.expect_re(rf"rows {initial_start} to {initial_stop} of {total}\. Enter")

# SGR wheel-up changes the independent viewport by three visual rows. It must
# reveal older command text without moving the commandline cursor.
sp.send("\x1b[<64;1;1M")
sp.expect_str("viewport-10")
sp.expect_re(r"rows 10 to 14 of 17")
sp.expect_str("\x1b[?25l")
sp.send(control("g"))

# A binding temporarily hands the tty away, then restores the still-requested
# viewport modes. Cancelling the command releases them for good.
sp.expect_str("\x1b[?25h")
sp.expect_str("\x1b[?1000l")
sp.expect_str("\x1b[?1006l")
sp.expect_str("\x1b[?1016;1015;1006;1005;1003;1002;1001;1000;9r")
sp.expect_str("\x1b[?9;1000;1001;1002;1003;1005;1006;1015;1016s")
sp.expect_str("\x1b[?1006h")
sp.expect_str("\x1b[?1000h")
sp.expect_str("\x1b[?25l")
sp.sleep(0.1)
sp.send(control("c"))
sp.expect_str("\x1b[?25h")
sp.expect_str("\x1b[?1000l")
sp.expect_str("\x1b[?1006l")
sp.expect_str("\x1b[?1016;1015;1006;1005;1003;1002;1001;1000;9r")

# Executing an overflowing command performs one final full repaint after any
# in-flight highlighting, so every visual row reaches terminal scrollback.
exec_commandline = (
    "\n".join(
        [
            "function viewport_exec",
            "# EXEC-FIRST",
            "# " + "y" * 150,
            "# EXEC-MIDDLE",
            "# " + "z" * 150,
            "# EXEC-LAST",
        ]
    )
    + "\n"
)
sp.send("\x1b[200~" + exec_commandline + "\x1b[201~")
sp.expect_re(r"rows \d+ to \d+ of \d+\. Enter")
assert "# EXEC-FIRST" not in sp.spawn.before
assert "# EXEC-MIDDLE" not in sp.spawn.before
sp.expect_str("\x1b[?1000h")
# One Enter completes and executes the command immediately; no viewport-confirmation
# Enter is required.
sp.sendline("end")
sp.expect_str("\x1b[?1000l")
sp.expect_str("\x1b[?1006l")
sp.expect_str("\x1b[?1016;1015;1006;1005;1003;1002;1001;1000;9r")
sp.expect_str("# EXEC-FIRST")
sp.expect_str("# EXEC-MIDDLE")
sp.expect_str("# EXEC-LAST")
sp.expect_str("\x1b[?25h")
sp.expect_prompt()
sp.sendline("functions -q viewport_exec; and echo single-enter-executed")
sp.expect_prompt("single-enter-executed")

sp.sendline(f"echo cursor=$viewport_cursor expected={len(commandline)}")
sp.expect_prompt(f"cursor={len(commandline)} expected={len(commandline)}")

# A single long logical line exercises automatic soft wrapping, which is the
# primary issue #11029 scenario. Wheel-down returns to the cursor, while a
# commandline edit exits overflow and gives the wheel back immediately.
soft_commandline = "# " + "x" * 240
sp.send("\x1b[200~" + soft_commandline + "\x1b[201~")
sp.expect_re(r"rows \d+ to \d+ of \d+")
sp.expect_str("\x1b[?1000h")
sp.send("\x1b[<64;1;1M")
sp.expect_str("\x1b[?25l")
sp.send("\x1b[<65;1;1M")
sp.expect_str("\x1b[?25h")
sp.send(control("u"))
sp.expect_str("\x1b[?1000l")
sp.expect_str("\x1b[?1006l")
sp.expect_str("\x1b[?1016;1015;1006;1005;1003;1002;1001;1000;9r")

# A pager search cannot retain input focus while the overflowing viewport withholds it. Clicking a
# command row must likewise keep subsequent input on the visible commandline.
sp.sendline("complete -c viewport_search -f -a 'alpha beta gamma'")
sp.expect_prompt()
sp.sendline(
    "bind ctrl-g 'set -g viewport_text (commandline); "
    "set -g viewport_focus (commandline --search-field 2>/dev/null; or echo commandline); "
    'set -g viewport_q (string match -q "*q*" -- $viewport_text; '
    "and echo q; or echo missing); "
    "set -g FISH_TEST_NO_RECURRENT_QUERIES 1; "
    'printf "\\e]0;viewport-focus-recorded:%s:%s:%s\\a" '
    "$viewport_pre_focus $viewport_focus $viewport_q'; "
    "bind ctrl-o 'set -g viewport_pre_focus "
    "(commandline --search-field 2>/dev/null; or echo commandline); "
    'printf "\\e]0;viewport-pre-focus=%s\\a" $viewport_pre_focus\''
)
sp.expect_prompt()
sp.sendline("set -e FISH_TEST_NO_RECURRENT_QUERIES; bind ctrl-l clear-screen")
sp.send(control("l"))
sp.expect_str("\x1b[6n")
sp.send_cursor_position_report(y=1, x=1)
sp.send_primary_device_attribute()
search_commandline = "viewport_search " + "arg " * 70
sp.send("\x1b[200~" + search_commandline + "\x1b[201~")
sp.expect_str("\x1b[?1000h")
sp.send("\t\t" + control("s") + "z")
sp.send(control("o"))
sp.expect_str("\x1b]0;viewport-pre-focus=commandline\x07")
sp.expect_str("\x1b[?1000h")
# Incremental repainting intentionally leaves the unchanged viewport status in
# place, so wait for the binding handoff and repaint to settle instead of
# requiring Fish to retransmit it.
sp.sleep(0.1)
read_available()
sp.send("\x1b[<0;1;1M")
sp.send("q" + control("g"))
sp.expect_str("\x1b]0;viewport-focus-recorded:commandline:commandline:q\x07")
sp.expect_str("\x1b[?1000h")
sp.sleep(0.1)
read_available()
sp.send(control("c"))
sp.expect_prompt()
sp.expect_str("\x1b[?1000l")
sp.expect_str("\x1b[?1006l")
sp.expect_str("\x1b[?1016;1015;1006;1005;1003;1002;1001;1000;9r")

# A nested key reader starts with its own default reader modes; the outer
# viewport request is restored only after it returns.
sp.sendline("bind ctrl-g fish_key_reader")
sp.expect_prompt()
sp.send("\x1b[200~" + soft_commandline + "\x1b[201~")
sp.expect_str("\x1b[?1000h")
sp.send("\x1b[<64;1;1M")
sp.expect_str("\x1b[?25l")
sp.send(control("g"))
sp.expect_str("\x1b[?25h")
sp.expect_str("\x1b[?1000l")
sp.expect_str("Press a key:")
sp.expect_str("\x1b[?1000h", shouldfail=True, timeout=0.1)
sp.expect_str("\x1b[?25l", shouldfail=True, timeout=0.1)
sp.send("q")
sp.expect_str("\x1b[?1000h")
sp.expect_str("\x1b[?25l")
sp.send(control("c"))
sp.expect_str("\x1b[?25h")
sp.expect_str("\x1b[?1000l")
sp.expect_str("\x1b[?1006l")
sp.expect_str("\x1b[?1016;1015;1006;1005;1003;1002;1001;1000;9r")
