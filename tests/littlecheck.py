#!/usr/bin/env python

""" Command line test driver. """

from __future__ import unicode_literals
from __future__ import print_function

import argparse
import datetime
import io
import re
import shlex
import subprocess
import sys

try:
    from itertools import zip_longest
except ImportError:
    from itertools import izip_longest as zip_longest
from difflib import SequenceMatcher

# Directives can occur at the beginning of a line, or anywhere in a line that does not start with #.
COMMENT_RE = r"^(?:[^#].*)?#\s*"

# A regex showing how to run the file.
RUN_RE = re.compile(COMMENT_RE + r"RUN:\s+(.*)\n")
REQUIRES_RE = re.compile(COMMENT_RE + r"REQUIRES:\s+(.*)\n")

# A regex capturing lines that should be checked against stdout.
CHECK_STDOUT_RE = re.compile(COMMENT_RE + r"CHECK:\s+(.*)\n")

# A regex capturing lines that should be checked against stderr.
CHECK_STDERR_RE = re.compile(COMMENT_RE + r"CHECKERR:\s+(.*)\n")

VARIABLE_OVERRIDE_RE = re.compile(r"\w+=.*")

SKIP = object()

def find_command(program):
    import os

    path, name = os.path.split(program)
    if path:
        return os.path.isfile(program) and os.access(program, os.X_OK)
    for path in os.environ["PATH"].split(os.pathsep):
        exe = os.path.join(path, program)
        if os.path.isfile(exe) and os.access(exe, os.X_OK):
            return exe

    return None

class Config(object):
    def __init__(self):
        # Whether to have verbose output.
        self.verbose = False
        # Whether output gets ANSI colorization.
        self.colorize = False
        # Whether to show which file was tested.
        self.progress = False

    def colors(self):
        """ Return a dictionary mapping color names to ANSI escapes """

        def ansic(n):
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


def output(*args):
    print("".join(args) + "\n")


import unicodedata


def esc(m):
    map = {
        "\n": "\\n",
        "\\": "\\\\",
        "\a": "\\a",
        "\b": "\\b",
        "\f": "\\f",
        "\r": "\\r",
        "\t": "\\t",
        "\v": "\\v",
    }
    if m in map:
        return map[m]
    if unicodedata.category(m)[0] == "C":
        return "\\x{:02x}".format(ord(m))
    else:
        return m


def escape_string(s):
    return "".join(esc(ch) for ch in s)


class CheckerError(Exception):
    """Exception subclass for check line parsing.

    Attributes:
      line: the Line object on which the exception occurred.
    """

    def __init__(self, message, line=None):
        super(CheckerError, self).__init__(message)
        self.line = line


class Line(object):
    """ A line that remembers where it came from. """

    def __init__(self, text, number, file):
        self.text = text
        self.number = number
        self.file = file

    def __hash__(self):
        # Chosen by fair diceroll
        # No, just kidding.
        # HACK: We pass this to the Sequencematcher, which puts the Checks into a dict.
        # To force it to match the regexes, we return a hash collision intentionally,
        # so it falls back on __eq__().
        #
        # CheckCmd has the same thing.
        return 0

    def __eq__(self, other):
        if other is None:
            return False
        if isinstance(other, CheckCmd):
            return other.regex.match(self.text)
        if isinstance(other, Line):
            # We only compare the text here so SequenceMatcher can reshuffle these
            return self.text == other.text
        raise NotImplementedError

    def subline(self, text):
        """ Return a substring of our line with the given text, preserving number and file. """
        return Line(text, self.number, self.file)

    @staticmethod
    def readfile(file, name):
        return [Line(text, idx + 1, name) for idx, text in enumerate(file)]

    def is_empty_space(self):
        return not self.text or self.text.isspace()

    def escaped_text(self, for_formatting=False):
        ret = escape_string(self.text.rstrip("\n"))
        if for_formatting:
            ret = ret.replace("{", "{{").replace("}", "}}")
        return ret


class RunCmd(object):
    """A command to run on a given Checker.

    Attributes:
        args: Unexpanded shell command as a string.
    """

    def __init__(self, args, line):
        self.args = args
        self.line = line

    @staticmethod
    def parse(line):
        if not shlex.split(line.text):
            raise CheckerError("Invalid RUN command", line)
        return RunCmd(line.text, line)


class TestFailure(object):
    def __init__(self, line, check, testrun, diff=None, lines=[], checks=[]):
        self.line = line
        self.check = check
        self.testrun = testrun
        self.error_annotation_lines = None
        self.diff = diff
        self.lines = lines
        self.checks = checks
        self.signal = None

    def message(self):
        fields = self.testrun.config.colors()
        fields["name"] = self.testrun.name
        fields["subbed_command"] = self.testrun.subbed_command
        if self.line:
            fields.update(
                {
                    "output_file": self.line.file,
                    "output_lineno": self.line.number,
                    "output_line": self.line.escaped_text(),
                }
            )
        if self.check:
            fields.update(
                {
                    "input_file": self.check.line.file,
                    "input_lineno": self.check.line.number,
                    "input_line": self.check.line.escaped_text(),
                    "check_type": self.check.type,
                }
            )
        filemsg = "" if self.testrun.config.progress else " in {name}"
        fmtstrs = ["{RED}Failure{RESET}" + filemsg + ":", ""]
        if self.signal:
            fmtstrs += [
                "  Process was killed by signal {BOLD}" + self.signal + "{RESET}",
                ""
            ]
        if self.line and self.check:
            fmtstrs += [
                "  The {check_type} on line {input_lineno} wants:",
                "    {BOLD}{input_line}{RESET}",
                "",
                "  which failed to match line {output_file}:{output_lineno}:",
                "    {BOLD}{output_line}{RESET}",
                "",
            ]

        elif self.check:
            fmtstrs += [
                "  The {check_type} on line {input_lineno} wants:",
                "    {BOLD}{input_line}{RESET}",
                "",
                "  but there was no remaining output to match.",
                "",
            ]
        else:
            fmtstrs += [
                "  There were no remaining checks left to match {output_file}:{output_lineno}:",
                "    {BOLD}{output_line}{RESET}",
                "",
            ]
        if self.error_annotation_lines:
            fields["error_annotation"] = "    ".join(
                [x.text for x in self.error_annotation_lines]
            )
            fields["error_annotation_lineno"] = str(
                self.error_annotation_lines[0].number
            )
            if len(self.error_annotation_lines) > 1:
                fields["error_annotation_lineno"] += ":" + str(
                    self.error_annotation_lines[-1].number
                )
            fmtstrs += [
                "  additional output on stderr:{error_annotation_lineno}:",
                "    {BOLD}{error_annotation}{RESET}",
            ]
        if self.diff:
            fmtstrs += ["  Context:"]
            lasthi = 0
            lastcheckline = None
            for d in self.diff.get_grouped_opcodes():
                for op, alo, ahi, blo, bhi in d:
                    color = "{BOLD}"
                    if op == "replace" or op == "delete":
                        color = "{RED}"
                    # We got a new chunk, so we print a marker.
                    if alo > lasthi:
                        fmtstrs += [
                            "    [...] from line "
                            + str(self.checks[blo].line.number)
                            + " ("
                            + self.lines[alo].file
                            + ":"
                            + str(self.lines[alo].number)
                            + "):"
                        ]
                    lasthi = ahi

                    # We print one "no more checks" after the last check and then skip any markers
                    lastcheck = False
                    for a, b in zip_longest(self.lines[alo:ahi], self.checks[blo:bhi]):
                        # Clean up strings for use in a format string - double up the curlies.
                        astr = (
                            color + a.escaped_text(for_formatting=True) + "{RESET}"
                            if a
                            else ""
                        )
                        if b:
                            bstr = (
                                "on line "
                                + str(b.line.number)
                                + ": {BLUE}"
                                + b.line.escaped_text(for_formatting=True)
                                + "{RESET}"
                            )
                            lastcheckline = b.line.number

                        if op == "equal":
                            fmtstrs += ["    " + astr]
                        elif b and a:
                            fmtstrs += [
                                "    "
                                + astr
                                + " <= does not match "
                                + b.type
                                + " "
                                + bstr
                            ]
                        elif b:
                            fmtstrs += [
                                "    "
                                + astr
                                + " <= nothing to match "
                                + b.type
                                + " "
                                + bstr
                            ]
                        elif not b:
                            string = "    " + astr
                            if bhi == len(self.checks):
                                if not lastcheck:
                                    string += " <= no more checks"
                                    lastcheck = True
                            elif lastcheckline is not None:
                                string += (
                                    " <= no check matches this, previous check on line "
                                    + str(lastcheckline)
                                )
                            else:
                                string += " <= no check matches"
                            fmtstrs.append(string)
            fmtstrs.append("")
        fmtstrs += ["  when running command:", "    {subbed_command}"]
        return "\n".join(fmtstrs).format(**fields)

    def print_message(self):
        """ Print our message to stdout. """
        print(self.message())


def perform_substitution(input_str, subs):
    """Perform the substitutions described by subs to str
    Return the substituted string.
    """
    # Sort our substitutions into a list of tuples (key, value), descending by length.
    # It needs to be descending because we need to try longer substitutions first.
    subs_ordered = sorted(subs.items(), key=lambda s: len(s[0]), reverse=True)

    def subber(m):
        # We get the entire sequence of characters.
        # Replace just the prefix and return it.
        text = m.group(1)
        for key, replacement in subs_ordered:
            if text.startswith(key):
                # shell-quote the replacement, so it's usable in #RUN lines.
                # We could loosen this and only do it for #RUN/#REQUIRES,
                # but so far we don't need it anywhere.
                return shlex.quote(replacement + text[len(key) :])
        # No substitution found, so we default to running it as-is,
        # which will end up running it via $PATH.
        return text

    return re.sub(r"%(%|[a-zA-Z0-9_-]+)", subber, input_str)


def runproc(cmd):
    """ Wrapper around subprocess.Popen to save typing """
    PIPE = subprocess.PIPE
    proc = subprocess.Popen(
        cmd,
        stdin=PIPE,
        stdout=PIPE,
        stderr=PIPE,
        shell=True,
        close_fds=True,  # For Python 2.6 as shipped on RHEL 6
    )
    return proc


class TestRun(object):
    def __init__(self, name, runcmd, checker, subs, config):
        self.name = name
        self.runcmd = runcmd
        self.subbed_command = perform_substitution(runcmd.args, subs)
        self.checker = checker
        self.subs = subs
        self.config = config

    def check(self, lines, checks):
        # Reverse our lines and checks so we can pop off the end.
        lineq = lines[::-1]
        checkq = checks[::-1]
        usedlines = []
        usedchecks = []
        mismatches = []
        while lineq and checkq:
            line = lineq[-1]
            check = checkq[-1]
            if check == line:
                # This line matched this checker, continue on.
                usedlines.append(line)
                usedchecks.append(check)
                lineq.pop()
                checkq.pop()
            elif line.is_empty_space():
                # Skip all whitespace input lines.
                lineq.pop()
            else:
                usedlines.append(line)
                usedchecks.append(check)
                mismatches.append((line, check))
                # Failed to match.
                lineq.pop()
                checkq.pop()

        # Drain empties
        while lineq and lineq[-1].is_empty_space():
            lineq.pop()

        # Store the remaining lines for the diff
        for i in lineq[::-1]:
            if not i.is_empty_space():
                usedlines.append(i)
        # Store remaining checks for the diff
        for i in checkq[::-1]:
            usedchecks.append(i)

        # If we have no more output, there's no reason to give
        # SCREENFULS of text.
        # So we truncate the check list.
        if len(usedchecks) > len(usedlines):
            usedchecks = usedchecks[:len(usedlines) + 5]

        # Do a SequenceMatch! This gives us a diff-like thing.
        diff = SequenceMatcher(a=usedlines, b=usedchecks, autojunk=False)
        # If there's a mismatch or still lines or checkers, we have a failure.
        # Otherwise it's success.
        if mismatches:
            return TestFailure(
                mismatches[0][0],
                mismatches[0][1],
                self,
                diff=diff,
                lines=usedlines,
                checks=usedchecks,
            )
        elif lineq:
            return TestFailure(
                lineq[-1], None, self, diff=diff, lines=usedlines, checks=usedchecks
            )
        elif checkq:
            return TestFailure(
                None, checkq[-1], self, diff=diff, lines=usedlines, checks=usedchecks
            )
        else:
            # Success!
            return None

    def run(self):
        """ Run the command. Return a TestFailure, or None. """

        def split_by_newlines(s):
            """Decode a string and split it by newlines only,
            retaining the newlines.
            """
            return [s + "\n" for s in s.decode("utf-8", errors="backslashreplace").split("\n")]

        if self.config.verbose:
            print(self.subbed_command)
        proc = runproc(self.subbed_command)
        stdout, stderr = proc.communicate()
        # HACK: This is quite cheesy: POSIX specifies that sh should return 127 for a missing command.
        # It's also possible that it'll be returned in other situations,
        # most likely when the last command in a shell script doesn't exist.
        # So we check if the command *we execute* exists, and complain then.
        status = proc.returncode
        cmd = next((word for word in shlex.split(self.subbed_command) if not VARIABLE_OVERRIDE_RE.match(word)))
        if status == 127 and not find_command(cmd):
            raise CheckerError("Command could not be found: " + cmd)
        if status == 126 and not find_command(cmd):
            raise CheckerError("Command is not executable: " + cmd)

        outlines = [
            Line(text, idx + 1, "stdout")
            for idx, text in enumerate(split_by_newlines(stdout))
        ]
        errlines = [
            Line(text, idx + 1, "stderr")
            for idx, text in enumerate(split_by_newlines(stderr))
        ]
        outfail = self.check(outlines, self.checker.outchecks)
        errfail = self.check(errlines, self.checker.errchecks)
        # It's possible that something going wrong on stdout resulted in new
        # text being printed on stderr. If we have an outfailure, and either
        # non-matching or unmatched stderr text, then annotate the outfail
        # with it.
        if outfail and errfail and errfail.line:
            outfail.error_annotation_lines = errlines[errfail.line.number - 1 :]
            # Trim a trailing newline
            if outfail.error_annotation_lines[-1].text == "\n":
                del outfail.error_annotation_lines[-1]
        failure = outfail if outfail else errfail

        if failure and status < 0:
            # Process was killed by a signal and failed,
            # add a message.
            import signal
            # Unfortunately strsignal only exists in python 3.8+,
            # and signal.signals is 3.5+.
            if hasattr(signal, "Signals"):
                try:
                    sig = signal.Signals(-status)
                    failure.signal = sig.name + " (" + signal.strsignal(sig.value) + ")"
                except ValueError:
                    failure.signal = str(-status)
            else:
                # No easy way to get the full list,
                # make up a dict.
                signals = {
                    signal.SIGABRT: "SIGABRT",
                    signal.SIGBUS: "SIGBUS",
                    signal.SIGFPE: "SIGFPE",
                    signal.SIGILL: "SIGILL",
                    signal.SIGSEGV: "SIGSEGV",
                    signal.SIGTERM: "SIGTERM",
                }
                failure.signal = signals.get(-status, str(-status))

        return failure


class CheckCmd(object):
    def __init__(self, line, checktype, regex):
        self.line = line
        self.type = checktype
        self.regex = regex

    def __hash__(self):
        # HACK: We pass this to the Sequencematcher, which puts the Checks into a dict.
        # To force it to match the regexes, we return a hash collision intentionally,
        # so it falls back on __eq__().
        #
        # Line has the same thing.
        return 0

    def __eq__(self, other):
        # "Magical" comparison with lines and strings.
        # Typically I wouldn't use this, but it allows us to check if a line matches any check in a dict or list via
        # the `in` operator.
        if other is None:
            return False
        if isinstance(other, CheckCmd):
            return self.regex == other.regex
        if isinstance(other, Line):
            return self.regex.match(other.text)
        if isinstance(other, str):
            return self.regex.match(other)
        raise NotImplementedError

    @staticmethod
    def parse(line, checktype):
        # type: (Line) -> CheckCmd
        # Everything inside {{}} is a regular expression.
        # Everything outside of it is a literal string.
        # Split around {{...}}. Then every odd index will be a regex, and
        # evens will be literals.
        # Note that if {{...}} appears first we will get an empty string in
        # the split array, so the {{...}} matches are always at odd indexes.
        bracket_re = re.compile(
            r"""
                \{\{   # Two open brackets
                (.*?)  # Nongreedy capture
                \}\}   # Two close brackets
            """,
            re.VERBOSE,
        )
        pieces = bracket_re.split(line.text)
        even = True
        re_strings = []
        for piece in pieces:
            if even:
                # piece is a literal string.
                re_strings.append(re.escape(piece))
            else:
                # piece is a regex (found inside {{...}}).
                # Verify the regex can be compiled.
                try:
                    re.compile(piece)
                except re.error:
                    raise CheckerError("Invalid regular expression: '%s'" % piece, line)
                re_strings.append(piece)
            even = not even
        # Enclose each piece in a non-capturing group.
        # This ensures that lower-precedence operators don't trip up catenation.
        # For example: {{b|c}}d would result in /b|cd/ which is different.
        # Backreferences are assumed to match across the entire string.
        re_strings = ["(?:%s)" % s for s in re_strings]
        # Anchor at beginning and end (allowing arbitrary whitespace), and maybe
        # a terminating newline.
        # We need the anchors because Python's match() matches an arbitrary prefix,
        # not the entire string.
        re_strings = [r"^\s*"] + re_strings + [r"\s*\n?$"]
        full_re = re.compile("".join(re_strings))
        return CheckCmd(line, checktype, full_re)


class Checker(object):
    def __init__(self, name, lines):
        self.name = name
        # Helper to yield subline containing group1 from all matching lines.
        def group1s(regex):
            for line in lines:
                m = regex.match(line.text)
                if m:
                    yield line.subline(m.group(1))

        # Find run commands.
        self.runcmds = [RunCmd.parse(sl) for sl in group1s(RUN_RE)]
        self.shebang_cmd = None
        if not self.runcmds:
            # If no RUN command has been given, fall back to the shebang.
            if lines[0].text.startswith("#!"):
                # Remove the "#!" at the beginning, and the newline at the end.
                cmd = lines[0].text[2:-1]
                self.shebang_cmd = cmd
                self.runcmds = [RunCmd(cmd + " %s", lines[0])]
            else:
                raise CheckerError("No runlines ('# RUN') found")

        self.requirecmds = [RunCmd.parse(sl) for sl in group1s(REQUIRES_RE)]

        # Find check cmds.
        self.outchecks = [
            CheckCmd.parse(sl, "CHECK") for sl in group1s(CHECK_STDOUT_RE)
        ]
        self.errchecks = [
            CheckCmd.parse(sl, "CHECKERR") for sl in group1s(CHECK_STDERR_RE)
        ]


def check_file(input_file, name, subs, config, failure_handler):
    """ Check a single file. Return a True on success, False on error. """
    success = True
    lines = Line.readfile(input_file, name)
    checker = Checker(name, lines)

    # Run all the REQUIRES lines first,
    # if any of them fail it's a SKIP
    for reqcmd in checker.requirecmds:
        proc = runproc(
            perform_substitution(reqcmd.args, subs)
        )
        proc.communicate()
        if proc.returncode > 0:
            return SKIP

    if checker.shebang_cmd is not None and not find_command(checker.shebang_cmd):
        raise CheckerError("Command could not be found: " + checker.shebang_cmd)

    # Only then run the RUN lines.
    for runcmd in checker.runcmds:
        failure = TestRun(name, runcmd, checker, subs, config).run()
        if failure:
            failure_handler(failure)
            success = False
    return success


def check_path(path, subs, config, failure_handler):
    with io.open(path, encoding="utf-8") as fd:
        return check_file(fd, path, subs, config, failure_handler)


def parse_subs(subs):
    """Given a list of input substitutions like 'foo=bar',
    return a dictionary like {foo:bar}, or exit if invalid.
    """
    result = {}
    for sub in subs:
        try:
            key, val = sub.split("=", 1)
            if not key:
                print("Invalid substitution %s: empty key" % sub)
                sys.exit(1)
            if not val:
                print("Invalid substitution %s: empty value" % sub)
                sys.exit(1)
            result[key] = val
        except ValueError:
            print("Invalid substitution %s: equal sign not found" % sub)
            sys.exit(1)
    return result


def get_argparse():
    """ Return a littlecheck argument parser. """
    parser = argparse.ArgumentParser(
        description="littlecheck: command line tool tester."
    )
    parser.add_argument(
        "-s",
        "--substitute",
        type=str,
        help="Add a new substitution for RUN lines. Example: bash=/bin/bash",
        action="append",
        default=[],
    )
    parser.add_argument(
        "-p",
        "--progress",
        action="store_true",
        dest="progress",
        help="Show the files to be checked",
        default=False,
    )
    parser.add_argument(
        "--force-color",
        action="store_true",
        dest="force_color",
        help="Force usage of color even if not connected to a terminal",
        default=False,
    )
    parser.add_argument("file", nargs="+", help="File to check")
    return parser


def main():
    args = get_argparse().parse_args()
    # Default substitution is %% -> %
    def_subs = {"%": "%"}
    def_subs.update(parse_subs(args.substitute))

    tests_count = 0
    failed = False
    skip_count = 0
    config = Config()
    config.colorize = args.force_color or sys.stdout.isatty()
    config.progress = args.progress
    fields = config.colors()

    for path in args.file:
        tests_count += 1
        fields["path"] = path
        if config.progress:
            print("Testing file {path} ... ".format(**fields), end="")
            sys.stdout.flush()
        subs = def_subs.copy()
        subs["s"] = path
        starttime = datetime.datetime.now()
        ret = check_path(path, subs, config, TestFailure.print_message)
        if ret is SKIP:
            skip_count += 1
        if not ret:
            failed = True
        elif config.progress:
            endtime = datetime.datetime.now()
            duration_ms = round((endtime - starttime).total_seconds() * 1000)
            reason = "ok"
            color = "{GREEN}"
            if ret is SKIP:
                reason = "SKIPPED"
                color = "{BLUE}"
            print(
                (color + "{reason}{RESET} ({duration} ms)").format(
                    duration=duration_ms, reason=reason, **fields
                )
            )

    # To facilitate integration with testing frameworks, use exit code 125 to indicate that all
    # tests have been skipped (primarily for use when tests are run one at a time). Exit code 125 is
    # used to indicate to automated `git bisect` runs that a revision has been skipped; we use it
    # for the same reasons git does.
    if skip_count > 0 and skip_count == tests_count:
        sys.exit(125)

    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
