# SPDX-FileCopyrightText: © 2020 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

function __fish_ps
    switch (builtin realpath (command -v ps) | string match -r '[^/]+$')
        case busybox
            command ps $argv
        case '*'
            command ps axc $argv
    end
end
