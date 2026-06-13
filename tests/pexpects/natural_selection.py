#!/usr/bin/env python3
# Tests for the text-editor-style selection bindings in fish_default_key_bindings:
# shift+movement extends a selection, typing replaces it, plain movement clears it,
# and backspace/delete remove it.
from pexpect_helper import SpawnedProc

sp = SpawnedProc()
sendline, expect_prompt, expect_str = sp.sendline, sp.expect_prompt, sp.expect_str
expect_prompt()

sendline("fish_default_key_bindings")
expect_prompt()

# Dump keys: print the current selection or the whole buffer. These override the
# generic binding for these characters, so they don't replace the selection themselves.
sendline("bind '~' 'echo -n \"S:<$(commandline --current-selection)>\"'")
expect_prompt()
sendline("bind '@' 'echo -n \"B:<$(commandline)>\"'")
expect_prompt()
dump_sel, dump_buf = "~", "@"

# Movement keys from the default bindings (these clear the selection).
home, end = "\x01", "\x05"  # Ctrl-A, Ctrl-E
right = "\x06"  # Ctrl-F
backspace = "\x7f"
delete = "\x1b[3~"

# Shift+movement (extends the selection).
s_right, s_left = "\x1b[1;2C", "\x1b[1;2D"
s_home, s_end = "\x1b[1;2H", "\x1b[1;2F"
as_left = "\x1b[1;4D"  # Alt+Shift+Left (extend by word)
as_right = "\x1b[1;4C"  # Alt+Shift+Right (extend by word)
cs_left = "\x1b[1;6D"  # Ctrl+Shift+Left (extend by word, same boundary as Ctrl+Left)
cs_right = "\x1b[1;6C"  # Ctrl+Shift+Right
ctrl_h = "\x08"  # Ctrl-H (emacs backspace alias, same helper as backspace)

# ---- shift+arrow extends by character ----
sendline("echo" + home + s_right + s_right + dump_sel)
expect_str("S:<ec>")
expect_prompt()
sendline("echo" + end + s_left + s_left + dump_sel)
expect_str("S:<ho>")
expect_prompt()

# ---- shift+home / shift+end select to line edges ----
sendline("echo" + end + s_home + dump_sel)
expect_str("S:<echo>")
expect_prompt()
sendline("echo" + home + s_end + dump_sel)
expect_str("S:<echo>")
expect_prompt()

# ---- alt+shift+left / alt+shift+right extend by word ----
sendline("echo abc" + as_left + dump_sel)
expect_str("S:<abc>")
expect_prompt()
sendline("echo abc" + home + as_right + dump_sel)
expect_str("S:<echo>")
expect_prompt()

# ---- ctrl+shift+left / ctrl+shift+right extend by word (same boundary as ctrl+arrow) ----
sendline("echo abc" + cs_left + dump_sel)
expect_str("S:<abc>")
expect_prompt()
sendline("echo abc" + home + cs_right + dump_sel)
expect_str("S:<echo>")
expect_prompt()

# ---- typing replaces the selection ----
sendline("echo abcd" + s_left + s_left + "XY" + dump_buf)
expect_str("B:<echo abXY>")
expect_prompt()
# ...including when the typed character is a space (an abbreviation-expanding key).
sendline("echo abcd" + s_left + s_left + " Z" + dump_buf)
expect_str("B:<echo ab Z>")
expect_prompt()

# ---- plain movement clears the selection ----
sendline("echo" + home + s_right + s_right + right + dump_sel)
expect_str("S:<>")
expect_prompt()

# ---- backspace / delete / ctrl-h remove the selection ----
sendline("echo abcd" + s_left + s_left + backspace + dump_buf)
expect_str("B:<echo ab>")
expect_prompt()
sendline("echo abcd" + s_left + s_left + delete + dump_buf)
expect_str("B:<echo ab>")
expect_prompt()
# ctrl-h shares the same __fish_kill_selection_or helper as backspace
sendline("echo abcd" + s_left + s_left + ctrl_h + dump_buf)
expect_str("B:<echo ab>")
expect_prompt()

# ---- fallthrough: backspace / delete still work with no selection ----
sendline("echo" + backspace + dump_buf)  # deletes trailing 'o'
expect_str("B:<ech>")
expect_prompt()
sendline("echo" + home + delete + dump_buf)  # deletes leading 'e'
expect_str("B:<cho>")
expect_prompt()
