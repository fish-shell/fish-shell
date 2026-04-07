# RUN: env fish_omitted_newline_char=● %fish %s | string match -r .
# Test custom omitted newline character via environment variable

echo -n test1
# CHECK: test1●
