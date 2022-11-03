# SPDX-FileCopyrightText: © 2018 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

function __fish_seen_argument --description 'Check whether argument is used'
    argparse --ignore-unknown 's/short=+' 'o/old=+' 'l/long=+' 'w/windows=+' -- $argv

    set --local tokens (commandline --current-process --tokenize --cut-at-cursor)
    set --erase tokens[1]

    for t in $tokens
        for s in $_flag_s
            if string match --regex --quiet -- "^-[A-z0-9]*"$s"[A-z0-9]*\$" $t
                return 0
            end
        end

        for o in $_flag_o
            if string match --quiet -- "-$o" $t
                return 0
            end
        end

        for l in $_flag_l
            if string match --quiet -- "--$l" $t
                return 0
            end
        end

        for w in $_flag_w
            if string match --quiet -- "/$w" $t
                return 0
            end
        end

        for raw_arg in $argv
            if string match --quiet -- $t $raw_arg
                return 0
            end
        end
    end

    return 1
end
