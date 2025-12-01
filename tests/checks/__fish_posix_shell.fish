# RUN: %fish %s

set -l sh (__fish_posix_shell)
command -v $sh
# CHECK: {{.*/sh}}
