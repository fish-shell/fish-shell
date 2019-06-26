#RUN: %fish %s
# Test various `bind` command invocations. This is meant to verify that
# invalid flags, mode names, etc. are caught as well as to verify that valid
# ones are allowed.

# Verify that an invalid bind mode is rejected. >&2
bind -m 'bad bind mode' \cX true
# CHECKERR: bind: mode name 'bad bind mode' is not valid. See `help identifiers`.
# Verify that an invalid bind mode target is rejected. >&2
bind -M bind-mode \cX true
# CHECKERR: bind: mode name 'bind-mode' is not valid. See `help identifiers`.
# CHECKERR: bind: No binding found for sequence '\t'

# This should succeed and result in a success, zero, status.
bind -M bind_mode \cX true

### HACK: All full bind listings need to have the \x7f -> backward-delete-char
# binding explicitly removed, because on some systems that's backspace, on others not.
# Listing bindings
bind | string match -v '*backward-delete-char'
bind --user --preset | string match -v '*backward-delete-char'
# CHECK: bind --preset '' self-insert
# CHECK: bind --preset \n execute
# CHECK: bind --preset \r execute
# CHECK: bind --preset \t complete
# CHECK: bind --preset \cc commandline\ \'\'
# CHECK: bind --preset \cd exit
# CHECK: bind --preset \ce bind
# CHECK: bind --preset \e\[A up-line
# CHECK: bind --preset \e\[B down-line
# CHECK: bind --preset \e\[C forward-char
# CHECK: bind --preset \e\[D backward-char
# CHECK: bind -M bind_mode \cx true
# CHECK: bind --preset '' self-insert
# CHECK: bind --preset \n execute
# CHECK: bind --preset \r execute
# CHECK: bind --preset \t complete
# CHECK: bind --preset \cc commandline\ \'\'
# CHECK: bind --preset \cd exit
# CHECK: bind --preset \ce bind
# CHECK: bind --preset \e\[A up-line
# CHECK: bind --preset \e\[B down-line
# CHECK: bind --preset \e\[C forward-char
# CHECK: bind --preset \e\[D backward-char
# CHECK: bind -M bind_mode \cx true

# Preset only
bind --preset | string match -v '*backward-delete-char'
# CHECK: bind --preset '' self-insert
# CHECK: bind --preset \n execute
# CHECK: bind --preset \r execute
# CHECK: bind --preset \t complete
# CHECK: bind --preset \cc commandline\ \'\'
# CHECK: bind --preset \cd exit
# CHECK: bind --preset \ce bind
# CHECK: bind --preset \e\[A up-line
# CHECK: bind --preset \e\[B down-line
# CHECK: bind --preset \e\[C forward-char
# CHECK: bind --preset \e\[D backward-char

# User only
bind --user | string match -v '*backward-delete-char'
# CHECK: bind -M bind_mode \cx true

# Adding bindings
bind \t 'echo banana'
bind | string match -v '*backward-delete-char'
# CHECK: bind --preset '' self-insert
# CHECK: bind --preset \n execute
# CHECK: bind --preset \r execute
# CHECK: bind --preset \t complete
# CHECK: bind --preset \cc commandline\ \'\'
# CHECK: bind --preset \cd exit
# CHECK: bind --preset \ce bind
# CHECK: bind --preset \e\[A up-line
# CHECK: bind --preset \e\[B down-line
# CHECK: bind --preset \e\[C forward-char
# CHECK: bind --preset \e\[D backward-char
# CHECK: bind -M bind_mode \cx true
# CHECK: bind \t 'echo banana'

# Erasing bindings
bind --erase \t
bind \t
bind \t 'echo wurst'
bind --erase --user --preset \t
bind \t
# CHECK: bind --preset \t complete

exit 0
