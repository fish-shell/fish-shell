"""pexpect_helper provides a wrapper around the pexpect module.

This module exposes a single class SpawnedProc, which wraps pexpect.spawn().
This exposes a pseudo-tty, which fish or another process may talk to.
The send() function may be used to send data to fish, and the expect_* family
of functions may be used to match what is output to the tty.

Example usage:
  sp = SpawnedProc() # this launches fish
  sp.expect_prompt() # wait for a prompt
  sp.sendline("echo hello world")
  sp.expect_prompt("hello world")

"""

from __future__ import print_function

import inspect
import os
import os.path
import re
import sys
import time
import pexpect
from signal import Signals

# Default timeout for failing to match.
TIMEOUT_SECS = 5

UNEXPECTED_SUCCESS = object()

# When rendering fish's output, remove the control sequences that modify terminal state,
# to avoid confusing the calling terminal.
SANITIZE_FOR_PRINTING_RE = re.compile(
    r"""
        # Filter CSI commands except for colors and cursor movement.
        (?!\x1b\[\d*m)
        (?!\x1b\[K)
        (?!\x1b\[\d*[ABCD])
        \x1b\[[\x30-\x3f]*[\x20-\x2f]*[\x40-\x7e]
        # OSC
        | \x1b\].*?\x07
        # DCS
        | \x1bP.*?\x1b\\
        # application keypad mode
        | \x1b=
        | \x1b>
    """,
    re.VERBOSE,
)

assert SANITIZE_FOR_PRINTING_RE.sub("", "\x1b[>4;1m") == ""
assert SANITIZE_FOR_PRINTING_RE.sub("", "\x1b[31m") == "\x1b[31m"


def get_prompt_re(counter):
    """Return a regular expression for matching a with a given prompt counter."""
    return re.compile("prompt %d>" % counter)


def get_callsite():
    """Return a triple (filename, line_number, line_text) of the call site location."""
    callstack = inspect.getouterframes(inspect.currentframe())
    for f in callstack:
        # Skip call sites from this file.
        if inspect.getmodule(f.frame) is Message.MODULE:
            continue
        # Skip functions which have a truthy callsite_skip attribute.
        if getattr(f.function, "callsite_skip", False):
            continue
        return (os.path.basename(f.filename), f.lineno, f.code_context)
    return ("Unknown", -1, "")


def escape(s):
    """Escape the string 's' to make it human-understandable."""
    res = []
    for c in s:
        if c == "\n":
            res.append("\\n")
        elif c == "\r":
            res.append("\\r")
        elif c == "\t":
            res.append("\\t")
        elif c.isprintable():
            res.append(c)
        else:
            res.append("\\x{:02x}".format(ord(c)))
    return "".join(res)


def pexpect_error_type(err):
    """Return a human-readable description of a pexpect error type."""
    if isinstance(err, pexpect.EOF):
        return "EOF"
    elif isinstance(err, pexpect.TIMEOUT):
        return "timeout"
    elif err is UNEXPECTED_SUCCESS:
        return "unexpected success"
    else:
        return "unknown error"


class Message(object):
    """Some text either sent-to or received-from the spawned proc.

    Attributes:
        dir: the message direction, either DIR_INPUT or DIR_OUTPUT
        filename: the name of the file from which the message was sent
        text: the text of the messages
        when: a timestamp of when the message was sent
    """

    # Input is input into fish shell ("sent data").
    DIR_INPUT = " INPUT"

    # Output means output from fish shell ("received data").
    DIR_OUTPUT = "OUTPUT"

    MODULE = sys.modules[__name__]

    def __init__(self, dir, text, when):
        """Construct from a direction, message text and timestamp."""
        self.dir = dir
        self.filename, self.lineno, _ = get_callsite()
        self.text = text
        self.when = when

    @staticmethod
    def sent_input(text, when):
        """Return an input message with the given text."""
        return Message(Message.DIR_INPUT, text, when)

    @staticmethod
    def received_output(text, when):
        """Return a output message with the given text."""
        return Message(Message.DIR_OUTPUT, text, when)


class SpawnedProc(object):
    """A process, talking to our ptty. This wraps pexpect.spawn.

    Attributes:
        colorize: whether error messages should have ANSI color escapes
        messages: list of Message sent and received, in-order
        start_time: the timestamp of the first message, or None if none yet
        spawn: the pexpect.spawn value
        prompt_counter: the index of the prompt. This cooperates with the fish_prompt
            function to ensure that each printed prompt is distinct.
    """

    def __init__(
        self,
        name="fish",
        timeout=TIMEOUT_SECS,
        env=os.environ.copy(),
        scroll_up_content_supported: bool = False,
        **kwargs,
    ):
        """Construct from a name, timeout, and environment.

        Args:
            name: the name of the executable to launch, as a key into the
                  environment dictionary. By default this is 'fish' but may be
                  other executables.
            timeout: A timeout to pass to pexpect. This indicates how long to wait
                     before giving up on some expected output.
            env: a string->string dictionary, describing the environment variables.
        """
        import shlex

        if name not in env:
            raise ValueError("'%s' variable not found in environment" % name)
        exe_path = env.get(name)
        # HACK: If there are no args, pexpect will fail if exe_path contains any shell metachars.
        # But not if there are args, in which case it probably switches spawning method?
        if "args" not in kwargs:
            exe_path = shlex.quote(exe_path)
        self.colorize = sys.stdout.isatty() or env.get("FISH_FORCE_COLOR", "0") == "1"
        self.messages = []
        self.start_time = None
        self.spawn = pexpect.spawn(
            exe_path, env=env, encoding="utf-8", timeout=timeout, **kwargs
        )
        self.spawn.delaybeforesend = None
        self.prompt_counter = 0
        if scroll_up_content_supported:
            # XTGETTCAP
            key = bytes.hex(b"indn")
            value = bytes.hex(b"dont-care")
            self.spawn.send(f"\x1bP1+r{key}={value}\x1b\\")
        if env.get("TERM") != "dumb":
            self.send_primary_device_attribute()

    def send_cursor_position_report(self, *, y: int, x: int):
        assert x != 0
        assert y != 0
        self.spawn.send(f"\x1b[{y};{x}R")

    def send_primary_device_attribute(self):
        self.spawn.send("\x1b[?123c")  # Primary Device Attribute

    def time_since_first_message(self):
        """Return a delta in seconds since the first message, or 0 if this is the first."""
        now = time.monotonic()
        if not self.start_time:
            self.start_time = now
        return now - self.start_time

    def send(self, s):
        """Cover over pexpect.spawn.send().
        Send the given string to the tty, returning the number of bytes written.
        """
        res = self.spawn.send(s)
        when = self.time_since_first_message()
        self.messages.append(Message.sent_input(s, when))
        return res

    def sendline(self, s):
        """Cover over pexpect.spawn.sendline().
        Send the given string + linesep to the tty, returning the number of bytes written.
        """
        return self.send(s + os.linesep)

    def expect_re(self, pat, pat_desc=None, unmatched=None, shouldfail=False, **kwargs):
        """Cover over pexpect.spawn.expect().
        Consume all "new" output of self.spawn until the given pattern is matched, or
        the timeout is reached.
        Note that output between the current position and the location of the match is
        consumed as well.
        The pattern is typically a regular expression in string form, but may also be
        any of the types accepted by pexpect.spawn.expect().
        If the 'unmatched' parameter is given, it is printed as part of the error message
        of any failure.
        On failure, this prints an error and exits.
        """
        try:
            self.spawn.expect(pat, **kwargs)
            when = self.time_since_first_message()
            self.messages.append(
                Message.received_output(self.spawn.match.group(), when)
            )
            # When a match is found,
            # spawn.match is the MatchObject that produced it.
            # This can be used to check what exactly was matched.
            if shouldfail:
                err = UNEXPECTED_SUCCESS
                if not pat_desc:
                    pat_desc = str(pat)
                self.report_exception_and_exit(pat_desc, unmatched, err)
            return self.spawn.match
        except pexpect.ExceptionPexpect as err:
            if shouldfail:
                return True
            if not pat_desc:
                pat_desc = str(pat)
            self.report_exception_and_exit(pat_desc, unmatched, err)

    def expect_str(self, s, **kwargs):
        """Cover over expect_re() which accepts a literal string."""
        return self.expect_re(re.escape(s), **kwargs)

    def expect_prompt(self, *args, increment=True, **kwargs):
        """Convenience function which matches some text and then a prompt.
        Match the given positional arguments as expect_re, and then look
        for a prompt.
        If increment is set, then this should be a new prompt and the prompt counter
        should be bumped; otherwise this is not a new prompt.
        Returns None on success, and exits on failure.
        Example:
           sp.sendline("echo hello world")
           sp.expect_prompt("hello world")
        """
        if args:
            self.expect_re(*args, **kwargs)
        if increment:
            self.prompt_counter += 1
        self.expect_re(
            get_prompt_re(self.prompt_counter),
            pat_desc="prompt %d" % self.prompt_counter,
        )

    def report_exception_and_exit(self, pat, unmatched, err):
        """Things have gone badly.
        We have an exception 'err', some pexpect.ExceptionPexpect.
        Report it to stdout, along with the offending call site.
        If 'unmatched' is set, print it to stdout.
        """
        # Close the process so we can get the status
        self.spawn.close()
        colors = self.colors()
        failtype = pexpect_error_type(err)
        # If we get an EOF, we check if the process exited with a signal.
        # This shows us e.g. if it crashed
        if (
            failtype == "EOF"
            and self.spawn.signalstatus is not None
            and self.spawn.signalstatus != 0
        ):
            failtype = "SIGNAL " + Signals(self.spawn.signalstatus).name

        fmtkeys = {"failtype": failtype, "pat": escape(pat)}
        fmtkeys.update(**colors)

        filename, lineno, code_context = get_callsite()
        fmtkeys["filename"] = filename
        fmtkeys["lineno"] = lineno
        fmtkeys["code"] = "\n".join([n.strip() for n in code_context if n])

        if unmatched:
            print(
                "{RED}Error: {NORMAL}{BOLD}{unmatched}{RESET}".format(
                    unmatched=unmatched, **fmtkeys
                )
            )
        print(
            "{RED}Failed to match pattern:{NORMAL} {BOLD}{pat}{RESET}".format(**fmtkeys)
        )
        print(
            "{filename}:{lineno}: {BOLD}{failtype}{RESET} from {code}".format(**fmtkeys)
        )

        print("")
        print("{CYAN}Escaped buffer:{RESET}".format(**colors))
        print(escape(self.spawn.before))
        print("")
        print("{CYAN}When written to the tty, this looks like:{RESET}".format(**colors))
        print("{CYAN}<-------{RESET}".format(**colors))
        sys.stdout.write(SANITIZE_FOR_PRINTING_RE.sub("", self.spawn.before))
        sys.stdout.flush()
        maybe_nl = ""
        if not self.spawn.before.endswith("\n"):
            maybe_nl = "\n{CYAN}(no trailing newline)".format(**colors)
        print(
            "{RESET}{maybe_nl}{CYAN}------->{RESET}".format(maybe_nl=maybe_nl, **colors)
        )

        print("")

        # Show the last 10 messages.
        print("Last 10 messages:")
        delta = None
        for m in self.messages[-10:]:
            etext = escape(m.text)
            timestamp = m.when * 1000.0
            # Use relative timestamps and add a sign.
            # This assumes a max length of 10^10 milliseconds (115 days) for the initial timestamp,
            # and 11.5 days for the delta.
            if delta:
                timestamp -= delta
                timestampstr = "{timestamp:+10.2f} ms".format(timestamp=timestamp)
            else:
                timestampstr = "{timestamp:10.2f} ms".format(timestamp=timestamp)
            delta = m.when * 1000.0
            print(
                "{dir} {timestampstr} (Line {lineno}): {BOLD}{etext}{RESET}".format(
                    dir=m.dir,
                    timestampstr=timestampstr,
                    filename=m.filename,
                    lineno=m.lineno,
                    etext=etext,
                    **colors,
                )
            )
        print("")
        sys.exit(1)

    def sleep(self, secs):
        """Cover over time.sleep()."""
        time.sleep(secs)

    def colors(self):
        """Return a dictionary mapping color names to ANSI escapes"""

        def ansic(n):
            """Return either an ANSI escape sequence for a color, or empty string."""
            return "\033[%dm" % n if self.colorize else ""

        return {
            "RESET": ansic(0),
            "BOLD": ansic(1),
            "NORMAL": ansic(39),
            "BLACK": ansic(30),
            "RED": ansic(31),
            "GREEN": ansic(32),
            "YELLOW": ansic(33),
            "BLUE": ansic(34),
            "MAGENTA": ansic(35),
            "CYAN": ansic(36),
            "LIGHTGRAY": ansic(37),
            "DARKGRAY": ansic(90),
            "LIGHTRED": ansic(91),
            "LIGHTGREEN": ansic(92),
            "LIGHTYELLOW": ansic(93),
            "LIGHTBLUE": ansic(94),
            "LIGHTMAGENTA": ansic(95),
            "LIGHTCYAN": ansic(96),
            "WHITE": ansic(97),
        }


def control(char: str) -> str:
    """Returns the char sent when control is pressed along the given key."""
    assert len(char) == 1
    char = char.lower()
    if ord("a") <= ord(char) <= ord("z"):
        return chr(ord(char) - ord("a") + 1)
    return chr(
        {
            "@": 0,
            "`": 0,
            "[": 27,
            "{": 27,
            "\\": 28,
            "|": 28,
            "]": 29,
            "}": 29,
            "^": 30,
            "~": 30,
            "_": 31,
            "?": 127,
        }[char]
    )
