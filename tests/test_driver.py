#!/usr/bin/env python3
import os
from datetime import datetime
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

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


def main():
    if len(sys.argv) < 2:
        print("Usage: test_driver.py FISH_DIRECTORY TESTS")
        return 1

    fishdir = Path(sys.argv[1]).absolute()
    if not fishdir.is_dir():
        fishdir = fishdir.parent
    script_path = Path(__file__).parent

    failcount = 0
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
            print(f"Binary does not exist: {bin}")
            return 127

    if len(sys.argv) > 2:
        files = [(os.path.abspath(path), path) for path in sys.argv[2:]]
    else:
        files = [
            (os.path.abspath(path), path.relative_to(script_path))
            for path in script_path.glob("checks/*.fish")
        ]
        files += [
            (os.path.abspath(path), path.relative_to(script_path))
            for path in script_path.glob("pexpects/*.py")
        ]

    # Set up tempdir
    # "delete=" was added in 3.12.
    home = tempfile.TemporaryDirectory(prefix="fishtest-")
    xdg_config = home.name + "/xdg_config_home"
    func_dir = xdg_config + "/fish/functions"
    os.makedirs(func_dir)
    os.makedirs(xdg_config + "/fish/conf.d/")
    for func in (script_path / "test_functions").glob("*.fish"):
        shutil.copy(func, func_dir + "/" + func.parts[-1])
    shutil.copy(
        script_path / "interactive.config", xdg_config + "/fish/conf.d/interactive.fish"
    )

    xdg_data = home.name + "/xdg_data_home"
    os.makedirs(xdg_data)
    xdg_runtime = home.name + "/xdg_runtime_home"
    os.makedirs(xdg_runtime)
    xdg_cache = home.name + "/xdg_cache_home"
    os.makedirs(xdg_cache)
    tmp = home.name + "/temp"
    os.makedirs(tmp)

    # Compile fish_test_helper if necessary.
    # If we're run multiple times, keep this around to save time.
    # TODO: It's cheesy to leave this in the current dir
    if not os.path.exists("fish_test_helper"):
        comp = subprocess.run(
            ["cc", script_path / "fish_test_helper.c", "-o", "fish_test_helper"]
        )
    shutil.copy("fish_test_helper", home.name + "/fish_test_helper")
    def_subs.update({"fish_test_helper": home.name + "/fish_test_helper"})

    # unset LANG, TERM, ...
    for var in [
        "XDG_DATA_DIRS",
        "LANGUAGE",
        "COLORTERM",
        "KONSOLE_PROFILE_NAME",
        "KONSOLE_VERSION",
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
            "HOME": home.name,
            "TMPDIR": tmp,
            "FISH_FAST_FAIL": "1",
            "FISH_UNIT_TESTS_RUNNING": "1",
            "XDG_CONFIG_HOME": xdg_config,
            "XDG_DATA_HOME": xdg_data,
            "XDG_RUNTIME_DIR": xdg_runtime,
            "XDG_CACHE_HOME": xdg_cache,
            "fish_test_helper": home.name + "/fish_test_helper",
            "TERM": "xterm",
            "LANG": "C",
            "LC_CTYPE": "en_US.UTF-8",
        }
    )

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

    os.chdir(home.name)

    print(f"Checking files (TMPDIR is {home.name})")
    if not PEXPECT and any(x.endswith(".py") for (x, _) in files):
        print(f"{RED}Skipping pexpect tests because pexpect is not installed{RESET}")

    for f, arg in files:
        starttime = datetime.now()
        if f.endswith(".fish"):
            subs = def_subs.copy()
            subs["s"] = f
            # littlecheck
            print(f"{arg}..", end="", flush=True)
            ret = littlecheck.check_path(f, subs, lconfig, lambda x: print(x.message()))
            endtime = datetime.now()
            duration_ms = round((endtime - starttime).total_seconds() * 1000)
            if ret is littlecheck.SKIP:
                print(f"{BLUE}SKIPPED{RESET}")
                skipcount += 1
            elif ret:
                print(f"{GREEN}PASS{RESET} ({duration_ms} ms)")
                passcount += 1
            else:
                print(f"{RED}FAIL{RESET} ({duration_ms} ms)")
                failcount += 1
        elif f.endswith(".py"):
            print(f"{arg}..", end="", flush=True)
            if not PEXPECT:
                print(f"{BLUE}SKIPPED{RESET}")
                skipcount += 1
                continue
            try:
                proc = subprocess.run(
                    ["python3", f],
                    capture_output=True,
                    env=pyenviron,
                    # Timeout of 90 seconds, about 9 times what any of these takes
                    timeout=90,
                )
            except subprocess.TimeoutExpired as e:
                print(f"{RED}FAILED due to timeout{RESET}")
                if e.output:
                    print(e.output.decode("utf-8"))
                if e.stderr:
                    print(e.stderr.decode("utf-8"))
                failcount += 1
                continue

            endtime = datetime.now()
            duration_ms = round((endtime - starttime).total_seconds() * 1000)
            if proc.returncode == 0:
                print(f"{GREEN}PASS{RESET} ({duration_ms} ms)")
                passcount += 1
            elif proc.returncode == 127:
                print(f"{BLUE}SKIPPED{RESET}")
                skipcount += 1
            else:
                print(f"{RED}FAILED{RESET} ({duration_ms} ms)")
                if proc.stdout:
                    print(proc.stdout.decode("utf-8"))
                if proc.stderr:
                    print(proc.stderr.decode("utf-8"))
                failcount += 1
        else:
            print(f"Not a valid test file: {arg}")
            failcount += 1
    if passcount + failcount + skipcount > 1:
        print(f"{passcount} / {passcount + failcount} passed ({skipcount} skipped)")
    if passcount == 0 and failcount == 0 and skipcount:
        return 125
    return 1 if failcount else 0


if __name__ == "__main__":
    try:
        ret = main()
        sys.exit(ret)
    except KeyboardInterrupt:
        sys.exit(130)
