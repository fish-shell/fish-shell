# SPDX-FileCopyrightText: © 2006 Axel Liljencrantz
# SPDX-FileCopyrightText: © 2009 fish-shell contributors
# SPDX-FileCopyrightText: © 2022 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

# This function is typically bound to Alt-L, it is used to list the contents
# of the directory under the cursor.

function __fish_list_current_token -d "List contents of token under the cursor if it is a directory, otherwise list the contents of the current directory"
    set -l val (commandline -t)
    printf "\n"
    if test -d $val
        ls $val
    else
        set -l dir (dirname -- $val)
        if test $dir != . -a -d $dir
            ls $dir
        else
            ls
        end
    end

    string repeat -N \n --count=(math (count (fish_prompt)) - 1)

    commandline -f repaint
end
