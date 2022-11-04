# SPDX-FileCopyrightText: © 2007 Axel Liljencrantz
# SPDX-FileCopyrightText: © 2009 fish-shell contributors
# SPDX-FileCopyrightText: © 2022 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

function __fish_complete_command --description 'Complete using all available commands'
    set -l ctoken (commandline -ct)
    switch $ctoken
        case '*=*'
            set ctoken (string split "=" -- $ctoken)
            printf '%s\n' $ctoken[1]=(complete -C "$ctoken[2]")
        case '-*' # do not try to complete options as commands
            return
        case '*'
            complete -C "$ctoken"
    end
end
