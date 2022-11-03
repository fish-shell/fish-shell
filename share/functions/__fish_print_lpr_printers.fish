# SPDX-FileCopyrightText: © 2012 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

function __fish_print_lpr_printers --description 'Print lpr printers'
    lpstat -p 2>/dev/null | sed 's/^\S*\s\(\S*\)\s\(.*\)$/\1\t\2/'


end
