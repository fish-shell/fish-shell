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

# Default timeout for failing to match.
TIMEOUT_SECS = 5


def get_prompt_re(counter):
    """ Return a regular expression for matching a with a given prompt counter. """
    return re.compile(
        r"""(?:\r\n?|^)   # beginning of line
            (?:\[.\]\ )?  # optional vi mode prompt
         """
        + (r"prompt\ %d>" % counter),  # prompt with counter
        re.VERBOSE,
    )


def get_callsite():
    """ Return a triple (filename, line_number, line_text) of the call site location. """
    callstack = inspect.getouterframes(inspect.currentframe())
    for f in callstack:
        if inspect.getmodule(f.frame) is not Message.MODULE:
            return (os.path.basename(f.filename), f.lineno, f.code_context)
    return ("Unknown", -1, "")


def escape(s):
    """ Escape the string 's' to make it human-understandable. """
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


class Message(object):
    """ Some text either sent-to or received-from the spawned proc.

    Attributes:
        dir: the message direction, either DIR_SEND or DIR_RECV
        filename: the name of the file from which the message was sent
        text: the text of the messages
        when: a timestamp of when the message was sent
    """

    DIR_SEND = "SENT"
    DIR_RECV = "RECV"
    MODULE = sys.modules[__name__]

    def __init__(self, dir, text, when):
        """ Construct from a direction, message text and timestamp. """
        self.dir = dir
        self.filename, self.lineno, _ = get_callsite()
        self.text = text
        self.when = when

    @staticmethod
    def sent(text, when):
        """ Return a SEND message with the given text. """
        return Message(Message.DIR_SEND, text, when)

    @staticmethod
    def received(text, when):
        """ Return a RECV message with the given text. """
        return Message(Message.DIR_RECV, text, when)

    def formatted(self):
        """ Return a human-readable string representing this message. """
        etext = escape(self.text)
        timestamp = self.when * 1000.0
        return "{dir} {timestamp:.2f} ({filename}:{lineno}): {etext}".format(
            timestamp=timestamp, etext=etext, **vars(self)
        )


class SpawnedProc(object):
    """ A process, talking to our ptty. This wraps pexpect.spawn.

    Attributes:
        colorize: whether error messages should have ANSI color escapes
        messages: list of Message sent and received, in-order
        start_time: the timestamp of the first message, or None if none yet
        spawn: the pexpect.spawn value
        prompt_counter: the index of the prompt. This cooperates with the fish_prompt
            function to ensure that each printed prompt is distinct.
    """

    def __init__(self, name="fish", timeout=TIMEOUT_SECS, env=os.environ.copy()):
        """ Construct from a name, timeout, and environment.

            Args:
                name: the name of the executable to launch, as a key into the
                      environment dictionary. By default this is 'fish' but may be
                      other executables.
                timeout: A timeout to pass to pexpect. This indicates how long to wait
                         before giving up on some expected output.
                env: a string->string dictionary, describing the environment variables.
        """
        if name not in env:
            raise ValueError("'name' variable not found in environment" % name)
        exe_path = env.get(name)
        self.colorize = sys.stdout.isatty()
        self.messages = []
        self.start_time = None
        self.spawn = pexpect.spawn(exe_path, env=env, encoding="utf-8", timeout=timeout)
        self.spawn.delaybeforesend = None
        self.prompt_counter = 1

    def time_since_first_message(self):
        """ Return a delta in seconds since the first message, or 0 if this is the first. """
        now = time.monotonic()
        if not self.start_time:
            self.start_time = now
        return now - self.start_time

    def send(self, s):
        """ Cover over pexpect.spawn.send().
            Send the given string to the tty, returning the number of bytes written.
        """
        res = self.spawn.send(s)
        when = self.time_since_first_message()
        self.messages.append(Message.sent(s, when))
        return res

    def sendline(self, s):
        """ Cover over pexpect.spawn.sendline().
            Send the given string + linesep to the tty, returning the number of bytes written.
        """
        return self.send(s + os.linesep)

    def expect_re(self, pat, pat_desc=None, unmatched=None, **kwargs):
        """ Cover over pexpect.spawn.expect().
            Look through the "new" output of self.spawn until the given pattern is matched.
            The pattern is typically a regular expression in string form, but may also be
            any of the types accepted by pexpect.spawn.expect().
            If the 'unmatched' parameter is given,
            On failure, this prints an error and exits.
        """
        try:
            res = self.spawn.expect(pat, **kwargs)
            when = self.time_since_first_message()
            self.messages.append(Message.received(self.spawn.match.group(), when))
            return res
        except pexpect.ExceptionPexpect as err:
            if not pat_desc:
                pat_desc = str(pat)
            self.report_exception_and_exit(pat_desc, unmatched, err)

    def expect_str(self, s, **kwargs):
        """ Cover over expect_re() which accepts a literal string. """
        return self.expect_re(re.escape(s), **kwargs)

    def expect_prompt(self, *args, **kwargs):
        """ Convenience function which matches some text and then a prompt.
            Match the given positional arguments as expect_re, and then look
            for a prompt, bumping the prompt counter.
            Returns None on success, and exits on failure.
            Example:
               sp.sendline("echo hello world")
               sp.expect_prompt("hello world")
        """
        if args:
            self.expect_re(*args, **kwargs)
        self.expect_re(
            get_prompt_re(self.prompt_counter),
            pat_desc="prompt %d" % self.prompt_counter,
        )
        self.prompt_counter += 1

    def report_exception_and_exit(self, pat, unmatched, err):
        """ Things have gone badly.
            We have an exception 'err', some pexpect.ExceptionPexpect.
            Report it to stdout, along with the offending call site.
            If 'unmatched' is set, print it to stdout.
        """
        colors = self.colors()
        if unmatched:
            print("{BOLD}{unmatched}{RESET}".format(unmatched=unmatched, **colors))
        if isinstance(err, pexpect.EOF):
            msg = "EOF"
        elif isinstance(err, pexpect.TIMEOUT):
            msg = "TIMEOUT"
        else:
            msg = "UNKNOWN"
        filename, lineno, code_context = get_callsite()
        print("{RED}Failed to match:{NORMAL} {pat}".format(pat=escape(pat), **colors))
        print(
            "{msg} from {filename}:{lineno}: {code}".format(
                msg=msg, filename=filename, lineno=lineno, code="\n".join(code_context)
            )
        )
        # Show the last 5 messages.
        for m in self.messages[-5:]:
            print(m.formatted())
        print("Buffer:")
        print(escape(self.spawn.before))
        sys.exit(1)

    def sleep(self, secs):
        """ Cover over time.sleep(). """
        time.sleep(secs)

    def colors(self):
        """ Return a dictionary mapping color names to ANSI escapes """

        def ansic(n):
            """ Return either an ANSI escape sequence for a color, or empty string. """
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
