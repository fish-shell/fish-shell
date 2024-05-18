#RUN: %fish %s
# NUL-handling

# This one actually prints a NUL
echo (printf '%s\x00' foo bar | string escape)
# CHECK: foo\x00bar\x00
# This one is truncated
echo foo\x00bar | string escape
# CHECK: foo
# This one is just escaped
echo foo\\x00bar | string escape
# CHECK: 'foo\\x00bar'
