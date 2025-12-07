#RUN: %fish %s
#REQUIRES: command -v diff

set -l fish (status fish-path)

set -l tmpdir (mktemp -d)
for bindings in true fish_default_key_bindings fish_vi_key_bindings
    $fish -c "
        $bindings
        bind > $tmpdir/old
        bind --erase --all --preset
        bind --erase --all
        source $tmpdir/old
        bind >$tmpdir/new
        diff -u $tmpdir/{old,new}
    "
end
echo >&2 bind output evaluation works
# CHECKERR: bind output evaluation works

# Test various `bind` command invocations. This is meant to verify that
# invalid flags, mode names, etc. are caught as well as to verify that valid
# ones are allowed.

# Verify that an invalid bind mode is rejected.
bind -m 'bad bind mode' \cX true
# CHECKERR: bind: bad bind mode: invalid mode name. See `help language#shell-variable-and-function-names`

# Verify that an invalid bind mode target is rejected.
bind -M bind-mode \cX true
# CHECKERR: bind: bind-mode: invalid mode name. See `help language#shell-variable-and-function-names`

# This should succeed and result in a success, zero, status.
bind -M bind_mode \cX true

# Listing bindings
bind | string match -v '*\e\\[*' # Hide raw bindings.
bind --user --preset | string match -v '*\e\\[*'
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
bind --preset | string match -v '*\e\\[*'
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
bind --user | string match -v '*\e\\[*'
# CHECK: bind -M bind_mode ctrl-x true

# Adding bindings
bind tab 'echo banana'
bind | string match -v '*\e\\[*'
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

bind ctrl-#
bind alt-\#
bind super-ctrl-~
bind super-alt-\~
# CHECKERR: bind: No binding found for key 'ctrl-#'
# CHECKERR: bind: No binding found for key 'alt-#'
# CHECKERR: bind: No binding found for key 'super-ctrl-~'
# CHECKERR: bind: No binding found for key 'super-alt-~'

# Legacy
bind \cx\cax 'echo foo'
bind \cx\cax
# CHECK: bind ctrl-x,ctrl-a,x 'echo foo'
bind \ef forward-word
bind \ef
# CHECK: bind alt-f forward-word


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

bind -k nul 'echo foo'
# CHECKERR: bind: the -k/--key syntax is no longer supported. See `bind --help` and `bind --key-names`

# Either Return or ctrl-m.
bind \r
# CHECK: bind --preset enter execute
# Never Return, probably always ctrl-j.
bind \n 2>&1
# CHECK: bind: No binding found for key 'ctrl-j'

bind _\cx_\ci_\ei_\\_\'_ 'echo foo'
# CHECKERR: bind: cannot parse key '_\cx_\t_\ei_\\_'_'

bind A
# CHECKERR: bind: No binding found for key 'A'

bind shift-a
# CHECKERR: bind: No binding found for key 'shift-a'

bind shift-A
# CHECKERR: bind: No binding found for key 'shift-A'

bind ctrl-shift-a
# CHECKERR: bind: No binding found for key 'ctrl-shift-a'

bind ctrl-shift-ä
# CHECKERR: bind: No binding found for key 'ctrl-shift-ä'

exit 0
