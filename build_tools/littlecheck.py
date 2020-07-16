#!/usr/bin/env python

""" Command line test driver. """

from __future__ import unicode_literals
from __future__ import print_function

import argparse
from collections import deque
import datetime
import io
import re
import shlex
import subprocess
import sys

# A regex showing how to run the file.
RUN_RE = re.compile(r"\s*#\s*RUN:\s+(.*)\n")

# A regex capturing lines that should be checked against stdout.
CHECK_STDOUT_RE = re.compile(r"\s*#\s*CHECK:\s+(.*)\n")

# A regex capturing lines that should be checked against stderr.
CHECK_STDERR_RE = re.compile(r"\s*#\s*CHECKERR:\s+(.*)\n")


class Config(object):
    def __init__(self):
        # Whether to have verbose output.
        self.verbose = False
        # Whether output gets ANSI colorization.
        self.colorize = False
        # Whether to show which file was tested.
        self.progress = False
        # How many after lines to print
        self.after = 5
        # How many before lines to print
        self.before = 5

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
        "'": "\\'",
        '"': '\\"',
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

    def subline(self, text):
        """ Return a substring of our line with the given text, preserving number and file. """
        return Line(text, self.number, self.file)

    @staticmethod
    def readfile(file, name):
        return [Line(text, idx + 1, name) for idx, text in enumerate(file)]

    def is_empty_space(self):
        return not self.text or self.text.isspace()


class RunCmd(object):
    """ A command to run on a given Checker.
    
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
    def __init__(self, line, check, testrun, before=None, after=None):
        self.line = line
        self.check = check
        self.testrun = testrun
        self.error_annotation_line = None
        # The output that comes *after* the failure.
        self.after = after
        self.before = before

    def message(self):
        afterlines = self.testrun.config.after
        fields = self.testrun.config.colors()
        fields["name"] = self.testrun.name
        fields["subbed_command"] = self.testrun.subbed_command
        if self.line:
            fields.update(
                {
                    "output_file": self.line.file,
                    "output_lineno": self.line.number,
                    "output_line": self.line.text.rstrip("\n"),
                }
            )
        if self.check:
            fields.update(
                {
                    "input_file": self.check.line.file,
                    "input_lineno": self.check.line.number,
                    "input_line": self.check.line.text,
                    "check_type": self.check.type,
                }
            )
        filemsg = "" if self.testrun.config.progress else " in {name}"
        fmtstrs = ["{RED}Failure{RESET}" + filemsg + ":", ""]
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
        if self.error_annotation_line:
            fields["error_annotation"] = self.error_annotation_line.text
            fields["error_annotation_lineno"] = self.error_annotation_line.number
            fmtstrs += [
                "  additional output on stderr:{error_annotation_lineno}:",
                "    {BOLD}{error_annotation}{RESET}",
            ]
        if self.before:
            fields["before_output"] = "    ".join(self.before)
            fields["additional_output"] = "    ".join(self.after[:afterlines])
            fmtstrs += [
                "  Context:",
                "    {BOLD}{before_output}    {RED}{output_line}{RESET} <= does not match '{LIGHTBLUE}{input_line}{RESET}'",
                "    {BOLD}{additional_output}{RESET}",
            ]
        elif self.after:
            fields["additional_output"] = "    ".join(self.after[:afterlines])
            fmtstrs += [
                "  Context:",
                "    {RED}{output_line}{RESET} <= does not match '{LIGHTBLUE}{input_line}{RESET}'",
                "    {BOLD}{additional_output}{RESET}"
            ]
        fmtstrs += ["  when running command:", "    {subbed_command}"]
        return "\n".join(fmtstrs).format(**fields)

    def print_message(self):
        """ Print our message to stdout. """
        print(self.message())


def perform_substitution(input_str, subs):
    """ Perform the substitutions described by subs to str
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
                return replacement + text[len(key) :]
        # No substitution found, so we default to running it as-is,
        # which will end up running it via $PATH.
        return text

    return re.sub(r"%(%|[a-zA-Z0-9_-]+)", subber, input_str)


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
        # We keep the last couple of lines in a deque so we can show context.
        before = deque(maxlen=self.config.before)
        while lineq and checkq:
            line = lineq[-1]
            check = checkq[-1]
            if check.regex.match(line.text):
                # This line matched this checker, continue on.
                lineq.pop()
                checkq.pop()
                before.append(line)
            elif line.is_empty_space():
                # Skip all whitespace input lines.
                lineq.pop()
            else:
                # Failed to match.
                lineq.pop()
                line.text = escape_string(line.text.strip()) + "\n"
                # Add context, ignoring empty lines.
                return TestFailure(
                    line,
                    check,
                    self,
                    before=[escape_string(line.text.strip()) + "\n" for line in before],
                    after=[
                        escape_string(line.text.strip()) + "\n"
                        for line in lineq[::-1]
                        if not line.is_empty_space()
                    ],
                )
        # Drain empties.
        while lineq and lineq[-1].is_empty_space():
            lineq.pop()
        # If there's still lines or checkers, we have a failure.
        # Otherwise it's success.
        if lineq:
            return TestFailure(lineq[-1], None, self)
        elif checkq:
            return TestFailure(None, checkq[-1], self)
        else:
            return None

    def run(self):
        """ Run the command. Return a TestFailure, or None. """

        def split_by_newlines(s):
            """ Decode a string and split it by newlines only,
                retaining the newlines.
            """
            return [s + "\n" for s in s.decode("utf-8").split("\n")]

        PIPE = subprocess.PIPE
        if self.config.verbose:
            print(self.subbed_command)
        proc = subprocess.Popen(
            self.subbed_command,
            stdin=PIPE,
            stdout=PIPE,
            stderr=PIPE,
            shell=True,
            close_fds=True,  # For Python 2.6 as shipped on RHEL 6
        )
        stdout, stderr = proc.communicate()
        # HACK: This is quite cheesy: POSIX specifies that sh should return 127 for a missing command.
        # Technically it's also possible to return it in other conditions.
        # Practically, that's *probably* not going to happen.
        status = proc.returncode
        if status == 127:
            raise CheckerError("Command could not be found: " + self.subbed_command)

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
            outfail.error_annotation_line = errfail.line
        return outfail if outfail else errfail


class CheckCmd(object):
    def __init__(self, line, checktype, regex):
        self.line = line
        self.type = checktype
        self.regex = regex

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
        if not self.runcmds:
            # If no RUN command has been given, fall back to the shebang.
            if lines[0].text.startswith("#!"):
                # Remove the "#!" at the beginning, and the newline at the end.
                self.runcmds = [RunCmd(lines[0].text[2:-1] + " %s", lines[0])]
            else:
                raise CheckerError("No runlines ('# RUN') found")

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
    """ Given a list of input substitutions like 'foo=bar',
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
    parser.add_argument("file", nargs="+", help="File to check")
    parser.add_argument(
        "-A",
        "--after",
        type=int,
        help="How many non-empty lines of output after a failure to print (default: 5)",
        action="store",
        default=5,
    )
    parser.add_argument(
        "-B",
        "--before",
        type=int,
        help="How many non-empty lines of output before a failure to print (default: 5)",
        action="store",
        default=5,
    )
    return parser


def main():
    args = get_argparse().parse_args()
    # Default substitution is %% -> %
    def_subs = {"%": "%"}
    def_subs.update(parse_subs(args.substitute))

    failure_count = 0
    config = Config()
    config.colorize = sys.stdout.isatty()
    config.progress = args.progress
    fields = config.colors()
    config.after = args.after
    config.before = args.before
    if config.before < 0:
        raise ValueError("Before must be at least 0")
    if config.after < 0:
        raise ValueError("After must be at least 0")

    for path in args.file:
        fields["path"] = path
        if config.progress:
            print("Testing file {path} ... ".format(**fields), end="")
            sys.stdout.flush()
        subs = def_subs.copy()
        subs["s"] = path
        starttime = datetime.datetime.now()
        if not check_path(path, subs, config, TestFailure.print_message):
            failure_count += 1
        elif config.progress:
            endtime = datetime.datetime.now()
            duration_ms = round((endtime - starttime).total_seconds() * 1000)
            print(
                "{GREEN}ok{RESET} ({duration} ms)".format(
                    duration=duration_ms, **fields
                )
            )
    sys.exit(failure_count)


if __name__ == "__main__":
    main()
