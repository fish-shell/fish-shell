# RUN: %fish %s

# Override the test-override again.
__fish_data_with_file functions/__fish_print_help.fish source

# REQUIRES: command -v sphinx-build
# REQUIRES: command -v man
# REQUIRES: test "$FISH_BUILD_DOCS" != "0"
# REQUIRES: %fish -c 'status build-info' | grep '^Features:.*embed-data'

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
