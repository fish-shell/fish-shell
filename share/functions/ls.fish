#
# Make ls use colors if we are on a system that supports that feature and writing to stdout.
#
if command ls --version >/dev/null ^/dev/null
    # This appears to be GNU ls.
    function ls --description "List contents of directory"
        set -l param --color=auto
        if isatty 1
            set param $param --indicator-style=classify
        end
        command ls $param $argv
    end

    if not set -q LS_COLORS
        if command -sq dircolors
            set -l colorfile
            for file in ~/.dir_colors ~/.dircolors /etc/DIR_COLORS
                if test -f $file
                    set colorfile $file
                    break
                end
            end
            # Here we rely on the legacy behavior of `dircolors -c` producing output suitable for
            # csh in order to extract just the data we're interested in.
            set -gx LS_COLORS (dircolors -c $colorfile | string split ' ')[3]
            # The value should always be quoted but be conservative and check first.
            if string match -qr '^([\'"]).*\1$' -- $LS_COLORS
                set LS_COLORS (string match -r '^.(.*).$' $LS_COLORS)[2]
            end
        end
    end
else if command ls -G / >/dev/null ^/dev/null
    # It looks like BSD, OS X and a few more which support colors through the -G switch instead.
    function ls --description "List contents of directory"
        command ls -G $argv
    end
end
