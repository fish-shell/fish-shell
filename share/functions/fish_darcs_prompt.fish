# Based on the Darc hub ``darcs-fish`` repositories of raichoo & toastal
function fish_darcs_prompt --description "Prompt function for Darcs"
    # No Darcs, no prompt
    command -sq darcs
    or return 1
    # The _darcs metadata directory, must be exist
    darcs show repo >/dev/null 2>&1
    or return 127

    # Basic green preferred over the logo color to clash less with terminal
    # emulator theming
    set -q fish_color_darcs_normal
    or set -l fish_color_darcs_normal green
    set -q fish_color_darcs_rebasing
    or set -l fish_color_darcs_rebasing yellow
    # Conflicts are a critical state that should be resolved
    set -q fish_color_darcs_conflict
    or set -l fish_color_darcs_conflict red

    # Darcs doesn’t have branches/channels so just showing the VCS name is more
    # consistent with having text as other VCS prompts do
    set -q fish_prompt_darcs_name
    or set -l fish_prompt_darcs_name darcs
    set -q fish_prompt_darcs_status_added
    or set -l fish_prompt_darcs_status_added '+'
    set -q fish_prompt_darcs_status_removed
    or set -l fish_prompt_darcs_status_removed -
    set -q fish_prompt_darcs_status_modified
    or set -l fish_prompt_darcs_status_modified '*'
    set -q fish_prompt_darcs_status_moved
    or set -l fish_prompt_darcs_status_moved '→'
    set -q fish_prompt_darcs_status_conflict
    or set -l fish_prompt_darcs_status_conflict '!'
    set -q fish_prompt_darcs_status_untracked
    or set -l fish_prompt_darcs_status_untracked '…'

    set -q fish_prompt_darcs_status_order
    or set -l fish_prompt_darcs_status_order added removed modified moved conflict untracked

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
