function __fish_set_lscolors --description 'Set $LS_COLORS if possible'
    if ! set -qx LS_COLORS && set -l cmd (command -s {g,}dircolors)[1]
        set -l colorfile
        for file in ~/.dir_colors ~/.dircolors /etc/DIR_COLORS
            if test -f $file
                set colorfile $file
                break
            end
        end
        # Here we rely on the legacy behavior of `dircolors -c` producing output
        # suitable for csh in order to extract just the data we're interested in.
        set -gx LS_COLORS ($cmd -c $colorfile | string split ' ')[3]
        # The value should always be quoted but be conservative and check first.
        if string match -qr '^([\'"]).*\1$' -- $LS_COLORS
            set LS_COLORS (string match -r '^.(.*).$' $LS_COLORS)[2]
        end
    end
end

function ls --description "List contents of directory"
    # Make ls use colors and show indicators if we are on a system that supports that feature and writing to stdout.
    #

    # BSD, macOS and others support colors with ls -G.
    # GNU ls and FreeBSD ls takes --color=auto. Order of this test is important because ls also takes -G but it has a different meaning.
    # Solaris 11's ls command takes a --color flag.
    # OpenBSD requires the separate colorls program for color support.
    # Also test -F because we'll want to define this function even with an ls that can't do colors (like NetBSD).
    if not set -q __fish_ls_color_opt
        set -g __fish_ls_color_opt
        set -g __fish_ls_command ls
        # OpenBSD ships a command called "colorls" that takes "-G" and "-F",
        # but there's also a ruby implementation that doesn't understand "-F".
        # Since that one's quite different, don't use it.
        if command -sq colorls
            and command colorls -GF >/dev/null 2>/dev/null
            set -g __fish_ls_color_opt -GF
            set -g __fish_ls_command colorls
        else
            for opt in --color=auto -G --color -F
                if command ls $opt / >/dev/null 2>/dev/null
                    set -g __fish_ls_color_opt $opt
                    break
                end
            end
        end
    end

    # Set the colors to the default via `dircolors` if none is given.
    __fish_set_lscolors

    isatty stdout
    and set -a opt -F

    # Terminal.app doesn't set $COLORTERM or $CLICOLOR,
    # but the new FreeBSD ls requires either to be set,
    # before it will enable color.
    # See #8309.
    # We don't set $COLORTERM because that should be set to
    # "truecolor" or similar and we don't want to specify that here.
    test "$TERM_PROGRAM" = Apple_Terminal
    and set -lx CLICOLOR 1

    command $__fish_ls_command $__fish_ls_color_opt $opt $argv
end
