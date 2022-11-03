# SPDX-FileCopyrightText: © 2014 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

function __fish_cursor_konsole -d 'Set cursor (konsole)'
    set -l shape $argv[1]
    switch "$shape"
        case block
            echo -en '\e]50;CursorShape=0\x7'
        case underscore
            echo -en '\e]50;CursorShape=2\x7'
        case line
            echo -en '\e]50;CursorShape=1\x7'
    end
end
