# SPDX-FileCopyrightText: © 2006 Axel Liljencrantz
#
# SPDX-License-Identifier: GPL-2.0-only

#
# Match colors for grep, if supported
#

if echo | command grep --color=auto "" >/dev/null 2>&1
    function grep
        command grep --color=auto $argv
    end
end
