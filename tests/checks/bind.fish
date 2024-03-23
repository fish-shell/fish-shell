#RUN: %fish %s
# Test various `bind` command invocations. This is meant to verify that
# invalid flags, mode names, etc. are caught as well as to verify that valid
# ones are allowed.

# Verify that an invalid bind mode is rejected.
bind -m 'bad bind mode' \cX true
# CHECKERR: bind: bad bind mode: invalid mode name. See `help identifiers`

# Verify that an invalid bind mode target is rejected.
bind -M bind-mode \cX true
# CHECKERR: bind: bind-mode: invalid mode name. See `help identifiers`

# This should succeed and result in a success, zero, status.
bind -M bind_mode \cX true

### HACK: All full bind listings need to have the \x7f -> backward-delete-char
# binding explicitly removed, because on some systems that's backspace, on others not.
# Listing bindings
bind | string match -v '*backward-delete-char'
bind --user --preset | string match -v '*backward-delete-char'
# CHECK: bind --preset '' self-insert
# CHECK: bind --preset [ret] execute
# CHECK: bind --preset [tab] complete
# CHECK: bind --preset [c-c] cancel-commandline
# CHECK: bind --preset [c-d] exit
# CHECK: bind --preset [c-e] bind
# CHECK: bind --preset [c-s] pager-toggle-search
# CHECK: bind --preset [c-u] backward-kill-line
# CHECK: bind --preset [up] up-line
# CHECK: bind --preset [down] down-line
# CHECK: bind --preset [right] forward-char
# CHECK: bind --preset [left] backward-char
# CHECK: bind --preset [c-p] up-line
# CHECK: bind --preset [c-n] down-line
# CHECK: bind --preset [c-b] backward-char
# CHECK: bind --preset [c-f] forward-char
# CHECK: bind -M bind_mode [c-x] true
# CHECK: bind --preset '' self-insert
# CHECK: bind --preset [ret] execute
# CHECK: bind --preset [tab] complete
# CHECK: bind --preset [c-c] cancel-commandline
# CHECK: bind --preset [c-d] exit
# CHECK: bind --preset [c-e] bind
# CHECK: bind --preset [c-s] pager-toggle-search
# CHECK: bind --preset [c-u] backward-kill-line
# CHECK: bind --preset [up] up-line
# CHECK: bind --preset [down] down-line
# CHECK: bind --preset [right] forward-char
# CHECK: bind --preset [left] backward-char
# CHECK: bind --preset [c-p] up-line
# CHECK: bind --preset [c-n] down-line
# CHECK: bind --preset [c-b] backward-char
# CHECK: bind --preset [c-f] forward-char
# CHECK: bind -M bind_mode [c-x] true

# Preset only
bind --preset | string match -v '*backward-delete-char'
# CHECK: bind --preset '' self-insert
# CHECK: bind --preset [ret] execute
# CHECK: bind --preset [tab] complete
# CHECK: bind --preset [c-c] cancel-commandline
# CHECK: bind --preset [c-d] exit
# CHECK: bind --preset [c-e] bind
# CHECK: bind --preset [c-s] pager-toggle-search
# CHECK: bind --preset [c-u] backward-kill-line
# CHECK: bind --preset [up] up-line
# CHECK: bind --preset [down] down-line
# CHECK: bind --preset [right] forward-char
# CHECK: bind --preset [left] backward-char
# CHECK: bind --preset [c-p] up-line
# CHECK: bind --preset [c-n] down-line
# CHECK: bind --preset [c-b] backward-char
# CHECK: bind --preset [c-f] forward-char

# User only
bind --user | string match -v '*backward-delete-char'
# CHECK: bind -M bind_mode [c-x] true

# Adding bindings
bind \t 'echo banana'
bind | string match -v '*backward-delete-char'
# CHECK: bind --preset '' self-insert
# CHECK: bind --preset [ret] execute
# CHECK: bind --preset [tab] complete
# CHECK: bind --preset [c-c] cancel-commandline
# CHECK: bind --preset [c-d] exit
# CHECK: bind --preset [c-e] bind
# CHECK: bind --preset [c-s] pager-toggle-search
# CHECK: bind --preset [c-u] backward-kill-line
# CHECK: bind --preset [up] up-line
# CHECK: bind --preset [down] down-line
# CHECK: bind --preset [right] forward-char
# CHECK: bind --preset [left] backward-char
# CHECK: bind --preset [c-p] up-line
# CHECK: bind --preset [c-n] down-line
# CHECK: bind --preset [c-b] backward-char
# CHECK: bind --preset [c-f] forward-char
# CHECK: bind -M bind_mode [c-x] true
# CHECK: bind [tab] 'echo banana'

# Erasing bindings
bind --erase \t
bind \t
bind \t 'echo wurst'
# CHECK: bind --preset [tab] complete
bind --erase --user --preset [tab]
bind \t
# CHECKERR: bind: No binding found for sequence '\t'

exit 0
