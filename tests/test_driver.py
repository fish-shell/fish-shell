#!/usr/bin/env python3
import argparse
from dataclasses import dataclass
from datetime import datetime
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
from typing import Optional

import littlecheck

try:
    import pexpect

    PEXPECT = True
except ImportError:
    PEXPECT = False

RESET = "\033[0m"
GREEN = "\033[32m"
BLUE = "\033[34m"
RED = "\033[31m"


def makeenv(script_path, home, test_helper_path):
    xdg_config = home + "/xdg_config_home"
    func_dir = xdg_config + "/fish/functions"
    os.makedirs(func_dir)
    os.makedirs(xdg_config + "/fish/conf.d/")
    for func in (script_path / "test_functions").glob("*.fish"):
        shutil.copy(func, func_dir + "/" + func.parts[-1])
    shutil.copy(
        script_path / "interactive.config", xdg_config + "/fish/conf.d/interactive.fish"
    )

    xdg_data = home + "/xdg_data_home"
    os.makedirs(xdg_data)
    xdg_runtime = home + "/xdg_runtime_home"
    os.makedirs(xdg_runtime)
    xdg_cache = home + "/xdg_cache_home"
    os.makedirs(xdg_cache)
    tmp = home + "/temp"
    os.makedirs(tmp)

    # Compile fish_test_helper if necessary.
    # If we're run multiple times, allow keeping this around to save time.
    if test_helper_path:
        thp = Path(test_helper_path)
        if not os.path.exists(thp / "fish_test_helper"):
            subprocess.run(
                [
                    "cc",
                    script_path / "fish_test_helper.c",
                    "-o",
                    thp / "fish_test_helper",
                ]
            )
        shutil.copy(thp / "fish_test_helper", home + "/fish_test_helper")
    else:
        subprocess.run(
            [
                "cc",
                script_path / "fish_test_helper.c",
                "-o",
                home + "/fish_test_helper",
            ]
        )

    # unset LANG, TERM, ...
    for var in [
        "XDG_DATA_DIRS",
        "LANGUAGE",
        "COLORTERM",
        "KONSOLE_VERSION",
        "TERM",  # Erase this since we still respect TERM=dumb etc.
        "TERM_PROGRAM",
        "TERM_PROGRAM_VERSION",
        "VTE_VERSION",
    ]:
        if var in os.environ:
            del os.environ[var]
    langvars = [key for key in os.environ.keys() if key.startswith("LC_")]
    for key in langvars:
        del os.environ[key]

    os.environ.update(
        {
            "HOME": home,
            "TMPDIR": tmp,
            "FISH_FAST_FAIL": "1",
            "FISH_UNIT_TESTS_RUNNING": "1",
            "XDG_CONFIG_HOME": xdg_config,
            "XDG_DATA_HOME": xdg_data,
            "XDG_RUNTIME_DIR": xdg_runtime,
            "XDG_CACHE_HOME": xdg_cache,
            "fish_test_helper": home + "/fish_test_helper",
            "LANG": "C",
            "LC_CTYPE": "en_US.UTF-8",
        }
    )


def main():
    if len(sys.argv) < 2:
        print("Usage: test_driver.py FISH_DIRECTORY TESTS")
        return 1

    script_path = Path(__file__).parent

    argparser = argparse.ArgumentParser(
        description="test_driver: Run fish's test suite"
    )
    argparser.add_argument(
        "-f",
        "--cachedir",
        type=str,
        help="Path to keep outputs to speed up the next run",
        action="store",
        default=None,
    )
    argparser.add_argument("fish", nargs=1, help="Fish to test")
    argparser.add_argument("file", nargs="*", help="Tests to run")
    args = argparser.parse_args()

    fishdir = Path(args.fish[0]).absolute()
    if not fishdir.is_dir():
        fishdir = fishdir.parent

    failcount = 0
    failed = []
    passcount = 0
    skipcount = 0
    def_subs = {"%": "%"}
    lconfig = littlecheck.Config()
    lconfig.colorize = sys.stdout.isatty()
    lconfig.progress = True

    for bin in ["fish", "fish_indent", "fish_key_reader"]:
        if os.path.exists(fishdir / bin):
            def_subs[bin] = str(fishdir / bin)
        else:
            print(f"Binary does not exist: {fishdir / bin}")
            return 127

    if args.file:
        files = [(os.path.abspath(path), path) for path in args.file]
    else:
        files = [
            (os.path.abspath(path), str(path.relative_to(script_path)))
            for path in sorted(script_path.glob("checks/*.fish"))
        ]
        files += [
            (os.path.abspath(path), str(path.relative_to(script_path)))
            for path in sorted(script_path.glob("pexpects/*.py"))
        ]

    if not PEXPECT and any(x.endswith(".py") for (x, _) in files):
        print(f"{RED}Skipping pexpect tests because pexpect is not installed{RESET}")

    longest_test_name_length = max([len(arg) for _, arg in files])
    max_expected_digits_duration = 5

    def print_result(arg, result, color, duration=None, suffix=None):
        duration_str = (
            ""
            if duration is None
            else f" {str(duration_ms).rjust(max_expected_digits_duration)} ms"
        )
        suffix_str = "" if suffix is None else f"\n{suffix}"
        print(
            f"{arg.ljust(longest_test_name_length)}  {color}{result}{RESET}  {duration_str}{suffix_str}"
        )

    tmp_root = tempfile.mkdtemp(prefix="fishtest-root-")

    for f, arg in files:
        match run_test(tmp_root, f, arg, script_path, args, def_subs, lconfig, fishdir):
            case TestSkip(arg):
                skipcount += 1
                print_result(arg, "SKIPPED", BLUE)
            case TestFail(arg, duration_ms, error_message):
                failcount += 1
                failed += [arg]
                print_result(arg, "FAILED", RED, duration_ms, error_message)
            case TestPass(arg, duration_ms):
                passcount += 1
                print_result(arg, "PASSED", GREEN, duration_ms)

    shutil.rmtree(tmp_root)

    if passcount + failcount + skipcount > 1:
        print(f"{passcount} / {passcount + failcount} passed ({skipcount} skipped)")
    if failcount:
        failstr = "\n    ".join(failed)
        print(f"{RED}Failed tests{RESET}: \n    {failstr}")
    if passcount == 0 and failcount == 0 and skipcount:
        return 125
    return 1 if failcount else 0


@dataclass
class TestSkip:
    arg: str


@dataclass
class TestFail:
    arg: str
    duration_ms: Optional[int]
    error_message: Optional[str]


@dataclass
class TestPass:
    arg: str
    duration_ms: int


TestResult = TestSkip | TestFail | TestPass


def run_test(
    tmp_root, path, arg, script_path, args, def_subs, lconfig, fishdir
) -> TestResult:
    if not path.endswith(".fish") and not path.endswith(".py"):
        return TestFail(arg, None, f"Not a valid test file: {arg}")

    starttime = datetime.now()
    home = tempfile.mkdtemp(prefix="fishtest-", dir=tmp_root)
    makeenv(script_path, home, args.cachedir)
    os.chdir(home)
    if path.endswith(".fish"):
        subs = def_subs.copy()
        subs.update({"s": path, "fish_test_helper": home + "/fish_test_helper"})

        # littlecheck
        ret = littlecheck.check_path(path, subs, lconfig, lambda x: print(x.message()))
        endtime = datetime.now()
        duration_ms = round((endtime - starttime).total_seconds() * 1000)
        if ret is littlecheck.SKIP:
            return TestSkip(arg)
        elif ret:
            return TestPass(arg, duration_ms)
        else:
            return TestFail(arg, duration_ms, f"Tmpdir is {home}")
    elif path.endswith(".py"):
        # environ for py files has a few changes.
        pyenviron = os.environ.copy()
        pyenviron.update(
            {
                "PYTHONPATH": str(script_path),
                "fish": str(fishdir / "fish"),
                "fish_key_reader": str(fishdir / "fish_key_reader"),
                "fish_indent": str(fishdir / "fish_indent"),
                "TERM": "dumb",
                "FISH_FORCE_COLOR": "1" if sys.stdout.isatty() else "0",
            }
        )
        if not PEXPECT:
            return TestSkip(arg)
        try:
            proc = subprocess.run(
                ["python3", path],
                capture_output=True,
                env=pyenviron,
                # Timeout of 120 seconds, about 10 times what any of these takes
                timeout=120,
            )
        except subprocess.TimeoutExpired as e:
            error_message = f"{RED}FAILED due to timeout{RESET}"
            if e.output:
                error_message += e.output.decode("utf-8")
            if e.stderr:
                error_message += e.stderr.decode("utf-8")
            return TestFail(arg, None, error_message)

        endtime = datetime.now()
        duration_ms = round((endtime - starttime).total_seconds() * 1000)
        if proc.returncode == 0:
            return TestPass(arg, duration_ms)
        elif proc.returncode == 127:
            return TestSkip(arg)
        else:
            error_message = ""
            if proc.stdout:
                error_message += proc.stdout.decode("utf-8")
            if proc.stderr:
                error_message += proc.stderr.decode("utf-8")
            error_message += f"Tmpdir is {home}"
            return TestFail(arg, duration_ms, error_message)
    else:
        return TestFail(arg, None, "Error in test driver. This should be unreachable.")


if __name__ == "__main__":
    try:
        ret = main()
        sys.exit(ret)
    except KeyboardInterrupt:
        sys.exit(130)
