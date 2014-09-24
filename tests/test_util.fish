# Utilities for the test runners

function die
    echo $argv[1] >&2
    exit 1
end

if not tty 0>&1 >/dev/null
    function set_color
        # do nothing
        return 0
    end
end

function say
    set -l color_flags
    set -l suppress_newline
    while set -q argv[1]
        switch $argv[1]
            case -b -o -u
                set color_flags $color_flags $argv[1]
            case -n
                set suppress_newline 1
            case --
                set -e argv[1]
                break
            case -\*
                continue
            case \*
                break
        end
        set -e argv[1]
    end

    if not set -q argv[1]
        echo 'usage: say [flags] color [string...]' >&2
        return 1
    end

    if set_color $color_flags $argv[1]
        set -e argv[1]
        echo -n $argv
        set_color reset
        if test -z "$suppress_newline"
            echo
        end
    end
end

function colordiff -d 'Colored diff output for unified diffs'
    diff $argv | while read -l line
        switch $line
            case '+*'
                say green $line
            case '-*'
                say red $line
            case '@*'
                say cyan $line
            case '*'
                echo $line
        end
    end
end
