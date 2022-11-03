# SPDX-FileCopyrightText: © 2017 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

function __fish_complete_user_at_hosts -d "Print list host-names with user@"
    for user_at in (commandline -ct | string match -r '.*@'; or echo "")(__fish_print_hostnames)
        echo $user_at
    end
end
