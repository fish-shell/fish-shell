#!/usr/bin/env python3

from pexpect_helper import SpawnedProc, control

sp = SpawnedProc(timeout=1)
sp.expect_prompt()

sp.sendline(" set fish_history ''")  # Use an ephemeral history.
sp.expect_prompt()

history = (
    ": 1 ABC",
    ": 2 AA",
    ": 3 ABC",
    ": 4 ABD",
    ": 5 ABC",
)

for line in history:
    sp.sendline(line)
    sp.expect_prompt()

### Normal substring history search.
# TODO use more discoverable key names, like "up-arrow" instead of "control-p".
sp.send("AB" + control("p"))
sp.expect_str("5 ABC")
sp.expect_str("5 ABC")  # Not sure why this one is painted twice.
sp.send(control("["))  # Escape goes back to the search term.
sp.expect_re(">AB\r") # Only the search term.
sp.send(control("w"))

# TERM=dumb does not support page-up (\033[5~), so we map them temporarily.
sp.sendline(" set -l unbind bind -e PageUp PageDown")  # Remember how to unbind without typing the sequences.
sp.sendline(" bind PageUp beginning-of-history; bind PageDown end-of-history")
sp.expect_prompt()
# Page Up/Down move to the first/last item that the current search visited.
sp.send("A" + 2 * control("p"))
sp.expect_re("5 ABC.*4 ABD")
sp.send(control("n"))
sp.expect_str("5 ABC")
sp.send("PageUp")
sp.expect_str("4 ABD")
sp.send("PageDown")  # Always moves to the search term.
sp.expect_str("A")
sp.send(control("a") + control("k"))
sp.sendline(" $unbind")
sp.expect_prompt()

# Do not move the cursor after a failing backward search (can be annoying with multiline commands).
sp.send('XYZ' + control("a"))
sp.send(" echo \r")
sp.expect_prompt()
sp.expect_prompt("\nXYZ\r")

### Modifiable search term.
sp.send("A" + control("p"))
sp.expect_str("5 ABC")
sp.send("A")  # The cursor is at the end of the match. Change the search term to "AA".
sp.expect_str("5 AABC")
sp.send(control("p"))
sp.expect_str("2 AA")
sp.send(control("n"))  # Forward search hit end, show the search term only.
sp.expect_str("AA")
sp.send(control("u"))

# Special case: search with empty string.
sp.send(control("p"))
sp.expect_str("5 ABC")
sp.send("A")  # The cursor is at the end of the line.
sp.expect_str("5 ABCA")
sp.send(control("p"))  # Search for "5 ABCA" which fails.
sp.expect_str("5 ABCA")
sp.send(control("n"))  # Also fails forward.
sp.expect_str("5 ABCA")
sp.send(control('u'))

### Incremental search.
sp.send(control("r") + "AA")  # Start incremental search.
sp.expect_str("2 AA")
sp.send(control("h") + "B")  # Change the search term to "AB", automatically go to the next match.
sp.expect_str("1 ABC")
sp.send(control("n"))  # Invert the search direction, to move forward in time.
sp.expect_str("3 ABC")
sp.send("D")
sp.expect_str("4 ABD")
sp.send(control("w") + control("p")) # Change the search term to "" (match anything).
sp.expect_str("3 ABC")
sp.send(control("a") + control("k"))


# Initialize incremental search with a non-empty search string, just like up-arrow search.
sp.send("BD" + control("r"))
sp.expect_str("4 ABD")
sp.send(control("[") + control("w"))

# Failing incremental search stops incremental mode. Not doing so would trigger a blocking history search on every keystroke that likely fails. Bash/Zsh handle failing incremental search by putting you in another special state, and as as soon as the user has hit backspace enough times so that the last matching search is restored, then incremental search is resumed. We don't want to have this special state that only allows ordinary characters and backspace. As a result it is harder to efficiently tell when the search string matches. Hence don't do that and have the user resume incremental mode manually.
sp.send(control("r") + "A")
sp.expect_str("5 ABC")
sp.send("x")  # Type "x" by accident.
sp.expect_str("5 AxBC")
sp.send(control("h") + "A")  # Correct the typo.
sp.expect_str("5 AABC")
sp.send(control("r"))  # Manually resume incremental mode.
sp.expect_str("2 AA")
sp.send(control("h") + "B")  # Check that we are in incremental mode.
sp.expect_str("1 ABC")
sp.send(control("a"))
sp.send(control("k"))
