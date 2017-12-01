# name: Informative Vcs
# author: Mariusz Smykula <mariuszs at gmail.com>
# human readable exit codes: Johan Walles <johan.walles at gmail.com>
function _print_exitcode --description 'Print a human-readable exit code'
    set -l exitcode $argv[1]
    if math $exitcode == 127 > /dev/null
        echo -n "Command not found"
        return
    end

    if math $exitcode \< 129 > /dev/null
        echo -n "$exitcode"
        return
    end

    set -l signal_number (math $exitcode - 128)

    # From "man signal" on macOS
    set -l signal_names[1] "SIGHUP"
    set -l signal_names[2] "SIGINT"
    set -l signal_names[9] "SIGKILL"
    set -l signal_names[13] "SIGPIPE"
    set -l signal_names[14] "SIGALRM"
    set -l signal_names[15] "SIGTERM"
    set -l signal_names[24] "SIGXCPU"
    set -l signal_names[25] "SIGXFSZ"
    set -l signal_names[26] "SIGVTALRM"
    set -l signal_names[27] "SIGPROF"
    set -l signal_names[30] "SIGUSR1"
    set -l signal_names[31] "SIGUSR2"

    # Interpretations
    set -l signal_names[2] "CTRL-C"
    set -l signal_names[9] "kill -9"
    set -l signal_names[15] "kill"

    set -l signal_name $signal_names[$signal_number]
    if [ "$signal_name" != "" ]
        echo -n "$signal_name"
        return
    end

    # We don't know, just print the exit code
    echo -n "$exitcode"
end

function fish_prompt --description 'Write out the prompt'
    set -l last_status $status

    if not set -q __fish_git_prompt_show_informative_status
        set -g __fish_git_prompt_show_informative_status 1
    end
    if not set -q __fish_git_prompt_hide_untrackedfiles
        set -g __fish_git_prompt_hide_untrackedfiles 1
    end

    if not set -q __fish_git_prompt_color_branch
        set -g __fish_git_prompt_color_branch magenta --bold
    end
    if not set -q __fish_git_prompt_showupstream
        set -g __fish_git_prompt_showupstream "informative"
    end
    if not set -q __fish_git_prompt_char_upstream_ahead
        set -g __fish_git_prompt_char_upstream_ahead "↑"
    end
    if not set -q __fish_git_prompt_char_upstream_behind
        set -g __fish_git_prompt_char_upstream_behind "↓"
    end
    if not set -q __fish_git_prompt_char_upstream_prefix
        set -g __fish_git_prompt_char_upstream_prefix ""
    end

    if not set -q __fish_git_prompt_char_stagedstate
        set -g __fish_git_prompt_char_stagedstate "●"
    end
    if not set -q __fish_git_prompt_char_dirtystate
        set -g __fish_git_prompt_char_dirtystate "✚"
    end
    if not set -q __fish_git_prompt_char_untrackedfiles
        set -g __fish_git_prompt_char_untrackedfiles "…"
    end
    if not set -q __fish_git_prompt_char_conflictedstate
        set -g __fish_git_prompt_char_conflictedstate "✖"
    end
    if not set -q __fish_git_prompt_char_cleanstate
        set -g __fish_git_prompt_char_cleanstate "✔"
    end

    if not set -q __fish_git_prompt_color_dirtystate
        set -g __fish_git_prompt_color_dirtystate blue
    end
    if not set -q __fish_git_prompt_color_stagedstate
        set -g __fish_git_prompt_color_stagedstate yellow
    end
    if not set -q __fish_git_prompt_color_invalidstate
        set -g __fish_git_prompt_color_invalidstate red
    end
    if not set -q __fish_git_prompt_color_untrackedfiles
        set -g __fish_git_prompt_color_untrackedfiles $fish_color_normal
    end
    if not set -q __fish_git_prompt_color_cleanstate
        set -g __fish_git_prompt_color_cleanstate green --bold
    end

    if not set -q __fish_prompt_normal
        set -g __fish_prompt_normal (set_color normal)
    end

    set -l color_cwd
    set -l prefix
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
            set suffix '$'
    end

    # PWD
    set_color $color_cwd
    echo -n (prompt_pwd)
    set_color normal

    printf '%s ' (__fish_vcs_prompt)

    if not test $last_status -eq 0
        set_color $fish_color_error
        echo -n "["
        _print_exitcode $last_status
        echo -n "] "
        set_color normal
    end

    echo -n "$suffix "
end
