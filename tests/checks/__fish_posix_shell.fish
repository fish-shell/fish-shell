# RUN: %fish %s

command -v (__fish_posix_shell)
# CHECK: {{.*/sh}}
