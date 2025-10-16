# RUN: %fish %s
# REQUIRES: command -v sphinx-build
# REQUIRES: command -v man
# REQUIRES: %fish -c 'status build-info' | grep '^Features:.*embed-data'

set -lx MANWIDTH 80
man abbr | head -n4
# CHECK: ABBR(1)                            fish-shell                           ABBR(1)
# CHECK: NAME
# CHECK:        abbr - manage fish abbreviations
man : | head -n4
# CHECK: TRUE(1)                            fish-shell                           TRUE(1)
# CHECK: NAME
# CHECK:        true - return a successful result
