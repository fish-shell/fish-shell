function ls --description "List contents of directory"
    # Make ls use colors and show indicators if we are on a system that supports that feature and writing to stdout.
    #

    # BSD, macOS and others support colors with ls -G.
    # GNU ls and FreeBSD ls takes --color=auto. Order of this test is important because ls also takes -G but it has a different meaning.
    # Solaris 11's ls command takes a --color flag.
    # OpenBSD requires the separate colorls program for color support.
    # Also test -F because we'll want to define this function even with an ls that can't do colors (like NetBSD).
    if not set -q __fish_ls_command
        set -g __fish_ls_command ls
        set -g __fish_ls_color_opt
        set -g __fish_ls_indicators_opt
        # OpenBSD ships a command called "colorls" that takes "-G" and "-F",
        # but there's also a ruby implementation that doesn't understand "-F".
        # Since that one's quite different, don't use it.
        if command -sq colorls
            and command colorls -GF >/dev/null 2>/dev/null
            set -g __fish_ls_command colorls
            set -g __fish_ls_color_opt -G
            set -g __fish_ls_indicators_opt -F
        else
            for opt in --color=auto -G --color
                if command ls $opt / >/dev/null 2>/dev/null
                    set -g __fish_ls_color_opt $opt
                    break
                end
            end

            if command ls -F / >/dev/null 2>/dev/null
                set -g __fish_ls_indicators_opt -F
            end
        end
    end

    set -l indicators_opt
    isatty stdout
    and set -a indicators_opt $__fish_ls_indicators_opt

    # Terminal.app doesn't set $COLORTERM or $CLICOLOR,
    # but the new FreeBSD ls requires either to be set,
    # before it will enable color.
    # See #8309.
    # We don't set $COLORTERM because that should be set to
    # "truecolor" or similar and we don't want to specify that here.
    test "$TERM_PROGRAM" = Apple_Terminal
    and set -lx CLICOLOR 1

    # If CLICOLOR_FORCE is set, don't colorize `ls` (if piped) because the results
    # might not be what we want; i.e. `ls --color=auto | cat` might still emit color
    # output (e.g. under BSD and macOS).
    # We don't just unset CLICOLOR_FORCE because the user might theoretically *want*
    # this behavior by explicitly including `--color=auto` in $argv themselves.
    set -qx CLICOLOR_FORCE && not isatty stdout; and set __fish_ls_color_opt

    command $__fish_ls_command $__fish_ls_color_opt $indicators_opt $argv
end
