# SPDX-FileCopyrightText: © 2006 Axel Liljencrantz
# SPDX-FileCopyrightText: © 2009 fish-shell contributors
# SPDX-FileCopyrightText: © 2022 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

function dirs --description 'Print directory stack'
    set -l options h/help c
    argparse -n dirs --max-args=0 $options -- $argv
    or return

    if set -q _flag_help
        __fish_print_help dirs
        return 0
    end

    if set -q _flag_c
        # Clear directory stack.
        set -e -g dirstack
        return 0
    end

    # Replace $HOME with ~.
    string replace -r '^'"$HOME"'($|/)' '~$1' -- $PWD $dirstack | string join " "
    return 0
end
