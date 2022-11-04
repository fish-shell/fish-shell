# SPDX-FileCopyrightText: © 2006 Axel Liljencrantz
# SPDX-FileCopyrightText: © 2009 fish-shell contributors
# SPDX-FileCopyrightText: © 2022 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

function __fish_use_subcommand -d "Test if a non-switch argument has been given in the current commandline"
    set -l cmd (commandline -poc)
    set -e cmd[1]
    for i in $cmd
        switch $i
            case '-*'
                continue
        end
        return 1
    end
    return 0
end
