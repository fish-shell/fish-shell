#
# Make ls use colors and show indicators if we are on a system that supports that feature and writing to stdout.
#

# BSD, macOS and others support colors with ls -G.
# GNU ls and FreeBSD ls takes --color=auto. Order of this test is important because ls also takes -G but it has a different meaning.
# Solaris 11's ls command takes a --color flag.
# Also test -F because we'll want to define this function even with an ls that can't do colors (like NetBSD).

for opt in --color=auto -G --color -F
    if command ls $opt / >/dev/null 2>/dev/null

        function ls --description "List contents of directory" -V opt
            isatty stdout
            and set -a opt -F
            command ls $opt $argv
        end

        if [ $opt = --color=auto ] && ! set -qx LS_COLORS && set -l cmd (command -s {g,}dircolors)[1]
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

        break
    end
end
