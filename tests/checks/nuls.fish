#RUN: %fish %s
# NUL-handling

echo foo\x00bar | string escape
# CHECK: foo\x00bar
echo foo\\x00bar | string escape
# CHECK: foo\\x00bar
