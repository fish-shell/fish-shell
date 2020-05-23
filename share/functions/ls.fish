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
            __fish_set_lscolors
            isatty stdout
            and set -a opt -F
            command ls $opt $argv
        end

        break
    end
end
