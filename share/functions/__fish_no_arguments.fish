# SPDX-FileCopyrightText: © 2006 Axel Liljencrantz
# SPDX-FileCopyrightText: © 2009 fish-shell contributors
# SPDX-FileCopyrightText: © 2022 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

function __fish_no_arguments -d "Internal fish function"
    set -l cmd (commandline -poc) (commandline -tc)
    set -e cmd[1]
    for i in $cmd
        switch $i
            case '-*'

            case '*'
                return 1
        end
    end
    return 0
end
