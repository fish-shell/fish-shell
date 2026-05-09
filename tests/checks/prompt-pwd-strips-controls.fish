#RUN: %fish %s

# Verify prompt_pwd strips terminal-control sequences from the path before
# returning. Protects prompts (and any other caller) from injection via
# hostile filenames in $PWD.

# Use dir-length=0 so prompt_pwd does not shorten the path.

# C0 + DEL: ESC, BEL, DEL
prompt_pwd -d 0 (printf '/tmp/a\x1bb\x07c\x7fd')
# CHECK: /tmp/abcd

# C1 (UTF-8 codepoints): U+009B (CSI) and U+009D (OSC)
prompt_pwd -d 0 (printf '/tmp/a\u009bb\u009dc')
# CHECK: /tmp/abc

# Raw single-byte C1 in a path (invalid UTF-8, fish stores as PUA).
prompt_pwd -d 0 (printf '/tmp/a\x9bb\x9dc')
# CHECK: /tmp/abc

# Latin-1 supplement (NBSP, ÿ) must be preserved.
prompt_pwd -d 0 (printf '/tmp/a\u00a0b\u00ffc')
# CHECK: /tmp/a{{.}}b{{.}}c
