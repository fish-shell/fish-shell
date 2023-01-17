# name: Disco
# author: Fabian Homborg

function fish_prompt
    set -l last_status $status

    set -l normal (set_color normal)
    set -l usercolor (set_color $fish_color_user)

    set -l delim \U25BA
    # If we don't have unicode use a simpler delimiter
    string match -qi "*.utf-8" -- $LANG $LC_CTYPE $LC_ALL; or set delim ">"

    fish_is_root_user; and set delim "#"

    set -l cwd (set_color $fish_color_cwd)
    if command -sq cksum
        # randomized cwd color
        # We hash the physical PWD and turn that into a color. That means directories (usually) get different colors,
        # but every directory always gets the same color. It's deterministic.
        # We use cksum because 1. it's fast, 2. it's in POSIX, so it should be available everywhere.
        set -l shas (pwd -P | cksum | string split -f1 ' ' | math --base=hex | string sub -s 3 | string pad -c 0 -w 6 | string match -ra ..)
        set -l col 0x$shas[1..3]

        # If the (simplified idea of) luminance is below 120 (out of 255), add some more.
        # (this runs at most twice because we add 60)
        while test (math 0.2126 x $col[1] + 0.7152 x $col[2] + 0.0722 x $col[3]) -lt 120
            set col[1] (math --base=hex "min(255, $col[1] + 60)")
            set col[2] (math --base=hex "min(255, $col[2] + 60)")
            set col[3] (math --base=hex "min(255, $col[3] + 60)")
        end
        set -l col (string replace 0x '' $col | string pad -c 0 -w 2 | string join "")

        set cwd (set_color $col)
    end

    # Prompt status only if it's not 0
    set -l prompt_status
    test $last_status -ne 0; and set prompt_status (set_color $fish_color_status)"[$last_status]$normal"

    # Only show host if in SSH or container
    # Store this in a global variable because it's slow and unchanging
    if not set -q prompt_host
        set -g prompt_host ""
        if set -q SSH_TTY
            or begin
                command -sq systemd-detect-virt
                and systemd-detect-virt -q
            end
            set prompt_host $usercolor$USER$normal@(set_color $fish_color_host)$hostname$normal":"
        end
    end

    # Shorten pwd if prompt is too long
    set -l pwd (prompt_pwd)

    echo -n -s $prompt_host $cwd $pwd $normal $prompt_status $delim
end

function fish_right_prompt
    set -g __fish_git_prompt_showdirtystate 1
    set -g __fish_git_prompt_showuntrackedfiles 1
    set -g __fish_git_prompt_showupstream informative
    set -g __fish_git_prompt_showcolorhints 1
    set -g __fish_git_prompt_use_informative_chars 1
    # Unfortunately this only works if we have a sensible locale
    string match -qi "*.utf-8" -- $LANG $LC_CTYPE $LC_ALL
    and set -g __fish_git_prompt_char_dirtystate \U1F4a9
    set -g __fish_git_prompt_char_untrackedfiles "?"

    # The git prompt's default format is ' (%s)'.
    # We don't want the leading space.
    set -l vcs (fish_vcs_prompt '(%s)' 2>/dev/null)

    set -l d (set_color brgrey)(date "+%R")(set_color normal)

    set -l duration "$cmd_duration$CMD_DURATION"
    if test $duration -gt 100
        set duration (math $duration / 1000)s
    else
        set duration
    end

    set -q VIRTUAL_ENV_DISABLE_PROMPT
    or set -g VIRTUAL_ENV_DISABLE_PROMPT true
    set -q VIRTUAL_ENV
    and set -l venv (string replace -r '.*/' '' -- "$VIRTUAL_ENV")

    set_color normal
    string join " " -- $venv $duration $vcs $d
end
