# SPDX-FileCopyrightText: © 2014 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

# vim: set ts=4 sw=4 tw=100 et:
# Utilities for the test runners

function die
    set -q argv[1]; and echo $argv[1] >&2
    exit 1
end

# $suppress_color is set by `test_driver.sh` (via import of exported variables)
function say -V suppress_color
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

    if not set -q argv[2]
        echo 'usage: say [flags] color string [string...]' >&2
        return 1
    end

    if begin
            test -n "$suppress_color"; or set_color $color_flags $argv[1]
        end
        printf '%s' $argv[2..-1]
        test -z "$suppress_color"; and set_color normal
        if test -z "$suppress_newline"
            echo
        end
    end
end

# lame timer
for program in {g,}date
    if command -q $program && $program --version 1>/dev/null 2>/dev/null
        set -g milli $program
        set -g unit ms
        break
    else
        set -g unit sec
    end
end

function timestamp
    set -q milli[1]
    and $milli +%s%3N
    or date +%s
end

function delta
    set -q milli[1]
    and math "( "($milli +%s%3N)" - $argv[1])"
    or math (date +%s) - $argv[1]
end
