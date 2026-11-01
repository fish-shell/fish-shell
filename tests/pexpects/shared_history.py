#!/usr/bin/env python3
"""Two live shells: shared searches must not merge ordinary session navigation."""

import os
from pathlib import Path

from pexpect_helper import SpawnedProc, control


env = os.environ.copy()
env["fish_history"] = "shared_search_test"
path = Path(env["XDG_DATA_HOME"]) / "fish" / "shared_search_test_history"
path.parent.mkdir(parents=True, exist_ok=True)
path.write_text("- cmd: echo SESSION_BASELINE\n  when: 1\n")

setup = r"""
set -e fish_history_search_shared
set -g fish_autosuggestion_enabled 0
bind ctrl-g 'printf "\nBUFFER:%s\n" (commandline)' clear-commandline
bind ctrl-o 'set -g fish_history_search_shared 1'
bind ctrl-b 'set -g fish_history_search_shared 0'
bind ctrl-v 'set -g fish_autosuggestion_enabled 1'
bind ctrl-p history-prefix-search-backward
bind ctrl-t history-token-search-backward
bind ctrl-l history-delete
"""
a = SpawnedProc(env=env.copy(), args=["-C", setup])
b = SpawnedProc(env=env.copy())
a.expect_prompt()
b.expect_prompt()


def inspect(text):
    a.send(control("g"))
    a.expect_str("BUFFER:" + text + "\r\n")
    a.sendline("")
    a.expect_prompt()


def run_b(command):
    b.sendline(command)
    b.expect_prompt()


run_b("echo SHARED_NEEDLE_one")

# Default behavior: neither empty navigation nor a typed search sees the append.
a.send("\033[A")
inspect("echo SESSION_BASELINE")
a.send("echo SHARED_NEEDLE_\033[A")
inspect("echo SHARED_NEEDLE_")

# Opt in without executing a command (which would change session navigation).
a.send(control("o"))
a.send("\033[A")
inspect("echo SESSION_BASELINE")
a.send("echo SHARED_NEEDLE_\033[A")
inspect("echo SHARED_NEEDLE_one")
a.send("echo SHARED_NEEDLE_" + control("p"))
inspect("echo SHARED_NEEDLE_one")
a.send("echo SHARED_NEEDLE_" + control("t"))
inspect("echo SHARED_NEEDLE_one")
# Down returns to the original query.
a.send("echo SHARED_NEEDLE_\033[A\033[B")
inspect("echo SHARED_NEEDLE_")

# A writer-side deletion replaces the file. Its replacement must be reloaded.
run_b(
    "builtin history delete --exact --case-sensitive 'echo SHARED_NEEDLE_one'; builtin history save"
)
a.send("echo SHARED_NEEDLE_" + control("p"))
inspect("echo SHARED_NEEDLE_")
run_b("echo SHARED_NEEDLE_two")
a.send("echo SHARED_NEEDLE_" + control("p"))
inspect("echo SHARED_NEEDLE_two")

# Deleting a shared match updates the frozen search and persists via A's history.
a.send("echo SHARED_NEEDLE_" + control("p") + control("l"))
inspect("echo SHARED_NEEDLE_")
a.send("echo SHARED_NEEDLE_" + control("p"))
inspect("echo SHARED_NEEDLE_")
run_b("builtin history merge")
b.sendline(
    "echo COUNT:(builtin history search --exact 'echo SHARED_NEEDLE_two' | count)"
)
b.expect_prompt("COUNT:0")

# Token deletion removes the originating command, including its other cached tokens.
run_b("echo SHARED_TOKEN_DELETE_first SHARED_TOKEN_DELETE_second")
a.send("echo SHARED_TOKEN_DELETE_" + control("t") + control("l"))
inspect("echo SHARED_TOKEN_DELETE_")
a.send("echo SHARED_TOKEN_DELETE_" + control("p"))
inspect("echo SHARED_TOKEN_DELETE_")
run_b("builtin history merge")
b.sendline(
    "echo COUNT:(builtin history search --exact 'echo SHARED_TOKEN_DELETE_first SHARED_TOKEN_DELETE_second' | count)"
)
b.expect_prompt("COUNT:0")

# New entries are also available as autosuggestions without a merge.
run_b("echo SHARED_SUGGESTION_three")
a.send(control("v") + "echo SHARED_SUGGESTION_")
a.sleep(0.2)  # The test runner uses TERM=dumb, which hides the grey preview.
a.send(control("f"))
inspect("echo SHARED_SUGGESTION_three")

# Switching the setting off restores session-only suggestions/searches.
a.send(control("b") + "echo SHARED_SUGGESTION_")
a.sleep(0.2)
a.send(control("f"))
inspect("echo SHARED_SUGGESTION_")
a.send("echo SHARED_SUGGESTION_" + control("p"))
inspect("echo SHARED_SUGGESTION_")
# Empty-line Up still has its original session view after all shared searches.
a.send("\033[A")
inspect("echo SESSION_BASELINE")

# A session-owned entry is removed from both the shared search and local navigation.
a.sendline("echo OWN_SHARED_DELETE")
a.expect_prompt()
a.send(control("o") + "echo OWN_SHARED_" + control("p") + control("l"))
inspect("echo OWN_SHARED_")
a.send("\033[A")
inspect("echo SESSION_BASELINE")

# Changing fish_history must discard the old shared cache; private history stays in memory.
a.sendline(" set fish_history ''")
a.expect_prompt()
a.sendline("echo PRIVATE_SESSION_ITEM")
a.expect_prompt()
a.send(control("o") + "echo PRIVATE_SESSION_" + control("p"))
inspect("echo PRIVATE_SESSION_ITEM")

a.sendline("exit")
b.sendline("exit")
