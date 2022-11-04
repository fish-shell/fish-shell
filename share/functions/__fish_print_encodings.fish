# SPDX-FileCopyrightText: © 2007 Axel Liljencrantz
# SPDX-FileCopyrightText: © 2009 fish-shell contributors
# SPDX-FileCopyrightText: © 2022 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

function __fish_print_encodings -d "Complete using available character encodings"
    if iconv --usage &>/dev/null # only GNU has this flag
        # Note: GNU iconv changes its output to contain the forward slashes below when stdout is not a tty.
        iconv --list | string replace -ra // ''
    else # non-GNU
        iconv --list | string replace -ra '\s+' '\n'
    end
end
