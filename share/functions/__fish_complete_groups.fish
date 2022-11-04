# SPDX-FileCopyrightText: © 2007 Axel Liljencrantz
# SPDX-FileCopyrightText: © 2009 fish-shell contributors
# SPDX-FileCopyrightText: © 2022 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

function __fish_complete_groups --description "Print a list of local groups, with group members as the description"
    if command -sq getent
        getent group | while read -l line
            string split -f 1,4 : -- $line | string join \t
        end
    else
        while read -l line
            string split -f 1,4 : -- $line | string join \t
        end </etc/group
    end
end
