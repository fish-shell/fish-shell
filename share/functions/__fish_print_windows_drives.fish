# SPDX-FileCopyrightText: © 2021 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

function __fish_print_windows_drives --description 'Print Windows drives'
    wmic logicaldisk get name | tail +2
end
