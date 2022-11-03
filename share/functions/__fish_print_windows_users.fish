# SPDX-FileCopyrightText: © 2022 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

function __fish_print_windows_users --description 'Print Windows user names'
    wmic useraccount get name | tail +2
end
