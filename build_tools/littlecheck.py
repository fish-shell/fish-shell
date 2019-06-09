#!/usr/bin/env python

""" Command line test driver. """

from __future__ import unicode_literals

import argparse
import io
import re
import shlex
import subprocess
import sys

# A regex showing how to run the file.
RUN_RE = re.compile(r"#\s*RUN:\s+(.*)\n")

# A regex capturing lines that should be checked against stdout.
CHECK_STDOUT_RE = re.compile(r"#\s*CHECK:\s+(.*)\n")

# A regex capturing lines that should be checked against stderr.
CHECK_STDERR_RE = re.compile(r"#\s*CHECKERR:\s+(.*)\n")


class Config(object):
    def __init__(self):
        # Whether to have verbose output.
        self.verbose = False
        # Whether output gets ANSI colorization.
        self.colorize = False

    def colors(self):
        """ Return a dictionary mapping color names to ANSI escapes """

        def ansic(n):
            return "\033[%dm" % n if self.colorize else ""

        return {
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
    def __init__(self, line, check, testrun):
        self.line = line
        self.check = check
        self.testrun = testrun

    def message(self):
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
                }
            )
        fmtstr = "{RED}Failure in {name}:{NORMAL}\n"
        if self.line and self.check:
            fmtstr += (
                "  Output line {output_file}:{output_lineno}: {output_line}\n"
                + "  failed to match line {input_lineno}: {input_line}\n"
            )
        elif self.check:
            fmtstr += (
                "Missing output for check:\n"
                + "  {input_file}:{input_lineno}: {input_line}\n"
            )
        else:
            fmtstr += (
                "Extra output after checks:\n"
                + "  Output line {output_file}:{output_lineno}: {output_line}\n"
            )
        fmtstr += "When running command:\n  {subbed_command}"
        return fmtstr.format(**fields)

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
        raise CheckerError("Unknown substitution: " + m.group(0))

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
        while lineq and checkq:
            line = lineq[-1]
            check = checkq[-1]
            if check.regex.match(line.text):
                # This line matched this checker, continue on.
                lineq.pop()
                checkq.pop()
            elif line.is_empty_space():
                # Skip all whitespace input lines.
                lineq.pop()
            else:
                # Failed to match.
                return TestFailure(line, check, self)
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
        def split_by_newlines(s):
            """ Decode a string and split it by newlines only,
                retaining the newlines.
            """
            return [s + "\n" for s in s.decode("utf-8").split("\n")]

        PIPE = subprocess.PIPE
        if self.config.verbose:
            print(self.subbed_command)
        proc = subprocess.Popen(
            self.subbed_command, stdin=PIPE, stdout=PIPE, stderr=PIPE, shell=True
        )
        stdout, stderr = proc.communicate()
        outlines = [
            Line(text, idx + 1, "stdout")
            for idx, text in enumerate(split_by_newlines(stdout))
        ]
        errlines = [
            Line(text, idx + 1, "stderr")
            for idx, text in enumerate(split_by_newlines(stderr))
        ]
        failure = self.check(outlines, self.checker.outchecks)
        if not failure:
            failure = self.check(errlines, self.checker.errchecks)
        return failure


class CheckCmd(object):
    def __init__(self, line, regex):
        self.line = line
        self.regex = regex

    @staticmethod
    def parse(line):
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
        return CheckCmd(line, full_re)


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
            raise CheckerError("No runlines ('# RUN') found")

        # Find check cmds.
        self.outchecks = [CheckCmd.parse(sl) for sl in group1s(CHECK_STDOUT_RE)]
        self.errchecks = [CheckCmd.parse(sl) for sl in group1s(CHECK_STDERR_RE)]


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
    parser.add_argument("file", nargs="+", help="File to check")
    return parser


def main():
    args = get_argparse().parse_args()
    # Default substitution is %% -> %
    def_subs = {"%": "%"}
    def_subs.update(parse_subs(args.substitute))

    success = True
    config = Config()
    config.colorize = sys.stdout.isatty()
    for path in args.file:
        subs = def_subs.copy()
        subs["s"] = path
        if not check_path(path, subs, config, TestFailure.print_message):
            success = False
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
