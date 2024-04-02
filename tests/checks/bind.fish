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

# Listing bindings
bind | string match -v '*escape,\\[*' # Hide legacy bindings.
bind --user --preset | string match -v '*escape,\\[*'
# CHECK: bind --preset '' self-insert
# CHECK: bind --preset enter execute
# CHECK: bind --preset tab complete
# CHECK: bind --preset ctrl-c cancel-commandline
# CHECK: bind --preset ctrl-d exit
# CHECK: bind --preset ctrl-e bind
# CHECK: bind --preset ctrl-s pager-toggle-search
# CHECK: bind --preset ctrl-u backward-kill-line
# CHECK: bind --preset backspace backward-delete-char
# CHECK: bind --preset up up-line
# CHECK: bind --preset down down-line
# CHECK: bind --preset right forward-char
# CHECK: bind --preset left backward-char
# CHECK: bind --preset ctrl-p up-line
# CHECK: bind --preset ctrl-n down-line
# CHECK: bind --preset ctrl-b backward-char
# CHECK: bind --preset ctrl-f forward-char
# CHECK: bind -M bind_mode ctrl-x true
# CHECK: bind --preset '' self-insert
# CHECK: bind --preset enter execute
# CHECK: bind --preset tab complete
# CHECK: bind --preset ctrl-c cancel-commandline
# CHECK: bind --preset ctrl-d exit
# CHECK: bind --preset ctrl-e bind
# CHECK: bind --preset ctrl-s pager-toggle-search
# CHECK: bind --preset ctrl-u backward-kill-line
# CHECK: bind --preset backspace backward-delete-char
# CHECK: bind --preset up up-line
# CHECK: bind --preset down down-line
# CHECK: bind --preset right forward-char
# CHECK: bind --preset left backward-char
# CHECK: bind --preset ctrl-p up-line
# CHECK: bind --preset ctrl-n down-line
# CHECK: bind --preset ctrl-b backward-char
# CHECK: bind --preset ctrl-f forward-char
# CHECK: bind -M bind_mode ctrl-x true

# Preset only
bind --preset | string match -v '*escape,\\[*'
# CHECK: bind --preset '' self-insert
# CHECK: bind --preset enter execute
# CHECK: bind --preset tab complete
# CHECK: bind --preset ctrl-c cancel-commandline
# CHECK: bind --preset ctrl-d exit
# CHECK: bind --preset ctrl-e bind
# CHECK: bind --preset ctrl-s pager-toggle-search
# CHECK: bind --preset ctrl-u backward-kill-line
# CHECK: bind --preset backspace backward-delete-char
# CHECK: bind --preset up up-line
# CHECK: bind --preset down down-line
# CHECK: bind --preset right forward-char
# CHECK: bind --preset left backward-char
# CHECK: bind --preset ctrl-p up-line
# CHECK: bind --preset ctrl-n down-line
# CHECK: bind --preset ctrl-b backward-char
# CHECK: bind --preset ctrl-f forward-char

# User only
bind --user | string match -v '*escape,\\[*'
# CHECK: bind -M bind_mode ctrl-x true

# Adding bindings
bind tab 'echo banana'
bind | string match -v '*escape,\\[*'
# CHECK: bind --preset '' self-insert
# CHECK: bind --preset enter execute
# CHECK: bind --preset tab complete
# CHECK: bind --preset ctrl-c cancel-commandline
# CHECK: bind --preset ctrl-d exit
# CHECK: bind --preset ctrl-e bind
# CHECK: bind --preset ctrl-s pager-toggle-search
# CHECK: bind --preset ctrl-u backward-kill-line
# CHECK: bind --preset backspace backward-delete-char
# CHECK: bind --preset up up-line
# CHECK: bind --preset down down-line
# CHECK: bind --preset right forward-char
# CHECK: bind --preset left backward-char
# CHECK: bind --preset ctrl-p up-line
# CHECK: bind --preset ctrl-n down-line
# CHECK: bind --preset ctrl-b backward-char
# CHECK: bind --preset ctrl-f forward-char
# CHECK: bind -M bind_mode ctrl-x true
# CHECK: bind tab 'echo banana'

# Erasing bindings
bind --erase tab
bind tab
bind tab 'echo wurst'
# CHECK: bind --preset tab complete
bind --erase --user --preset tab
bind tab
# CHECKERR: bind: No binding found for key 'tab'

bind ctrl-\b
# CHECKERR: bind: Cannot add control modifier to control character 'ctrl-h'

exit 0
