# name: Classic + Status
# author: David Frascone

function fish_prompt --description "Write out the prompt"
    # Save our status
    set -l last_pipestatus $pipestatus
    set -l last_status $status

    # only output $pipestatus if there was a pipe and any part of it had non-zero exit status
    # TODO maybe have a common function that returns true if all array elements match a certain value?
    if test (count $last_pipestatus) -gt 1
        for pstat in $last_pipestatus
            if test $pstat -ne 0
                set -l last_pipestatus_string (string join "|" (__fish_pipestatus_with_signal $last_pipestatus))
                printf "%s[%s]%s " (set_color yellow --bold) $last_pipestatus_string (set_color normal)
                break
            end
        end
    end

    if test $last_status -ne 0
        printf "%s(%s)%s " (set_color red --bold) (__fish_status_to_signal $last_status) (set_color normal)
    end

    set -l color_cwd
    set -l suffix
    switch "$USER"
        case root toor
            if set -q fish_color_cwd_root
                set color_cwd $fish_color_cwd_root
            else
                set color_cwd $fish_color_cwd
            end
            set suffix '#'
        case '*'
            set color_cwd $fish_color_cwd
            set suffix '>'
    end

    echo -n -s "$USER" @ (prompt_hostname) ' ' (set_color $color_cwd) (prompt_pwd) (set_color normal) "$suffix "
end
