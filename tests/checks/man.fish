# RUN: %fish %s

# REQUIRES: command -v sphinx-build
# REQUIRES: command -v man
# REQUIRES: %fish -c '__fish_data_with_directory man/man1 ".*" ls' | grep -q .

# Override the test-override again.
status get-file functions/__fish_print_help.fish | source

set -l deroff col -b -p -x

set -lx MANWIDTH 80
man abbr | $deroff | head -n4
# CHECK: ABBR(1) {{ *}} fish-shell {{ *}} ABBR(1)
# CHECK: NAME
# CHECK:        abbr - manage fish abbreviations
man : | $deroff | head -n4
# CHECK: TRUE(1) {{ *}} fish-shell {{ *}} TRUE(1)
# CHECK: NAME
# CHECK:        true - return a successful result
