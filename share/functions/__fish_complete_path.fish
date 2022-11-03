# SPDX-FileCopyrightText: © 2014 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

function __fish_complete_path --description "Complete using path"
    set -l target
    set -l description
    switch (count $argv)
        case 0
            # pass
        case 1
            set target "$argv[1]"
        case 2 "*"
            set target "$argv[1]"
            set description "$argv[2]"
    end
    set -l targets "$target"*
    if set -q targets[1]
        printf "%s\t$description\n" (command ls -dp $targets)
    end
end
