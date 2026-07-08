# Based on the Darc hub ``darcs-fish`` repositories of raichoo & toastal

# For a Darcs-themed prompt try:
# set fish_color_darcs_normal $fish_darcs_logo_color
set -g fish_darcs_logo_color 72ff01
# Basic green preferred to clash less with terminal emulator theming
set -g fish_color_darcs_normal green
set -g fish_color_darcs_rebasing yellow
# Conflicts are a critical state that should be resolved
set -g fish_color_darcs_conflict red

# Darcs doesn’t have branches/channels so just showing the VCS name is more
# consistent with having text as other VCS prompts do
set -g fish_prompt_darcs_name darcs

set -g fish_prompt_darcs_status_added '+'
set -g fish_prompt_darcs_status_removed -
set -g fish_prompt_darcs_status_modified '*'
set -g fish_prompt_darcs_status_moved '→'
set -g fish_prompt_darcs_status_conflict '!'
set -g fish_prompt_darcs_status_untracked '…'

set -g fish_prompt_darcs_status_order added removed modified moved conflict untracked

function fish_darcs_prompt --description "Prompt function for Darcs"
    # No Darcs, no prompt
    command -sq darcs || return 1
    # The _darcs metadata directory, must be exist
    darcs show repo >/dev/null 2>&1 || return 1

    set -l added 0
    set -l modified 0
    set -l removed 0
    set -l moved 0
    set -l untracked 0
    set -l conflict 0

    for state_line in (darcs status --machine-readable 2>&1)
        set -l parts (string split ' ' -- "$state_line")
        switch $parts[1]
            case Rebase
                set rebase $parts[4]
            case A
                set added (math "$added + 1")
            case M
                set modified (math "$modified + 1")
            case R
                set removed (math "$removed + 1")
            case F
                set moved (math "$moved + 1")
            case a
                set untracked (math "$untracked + 1")
            case '!'
                set conflict (math "$conflict + 1")
        end
    end

    set_color normal
    echo -n '('
    if set -q rebase
        set_color $fish_color_darcs_rebasing
    else if test "$conflict" -gt 0
        set_color $fish_color_darcs_conflict
    else
        set_color $fish_color_darcs_normal
    end
    echo -n $fish_prompt_darcs_name
    set_color normal
    if set -q rebase
        echo -n "[$rebase]"
    end

    set -l sep
    if string length -q -- "$fish_prompt_darcs_name"
        set sep '|'
    end
    for stat in $fish_prompt_darcs_status_order
        set -l count $$stat
        if test "$count" -gt 0
            echo -n "$sep"
            set -l color_var fish_color_darcs_$stat
            if set -q $color_var
                set_color $$color_var
            end
            echo -n "$count"
            set -l symbol_var fish_prompt_darcs_status_$stat
            echo -n "$$symbol_var"
            set_color normal
            set sep /
        end
    end
    set_color normal
    echo -n ')'
end
