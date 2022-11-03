# SPDX-FileCopyrightText: © 2019 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

# returns 0 only if any argument is on of the supplied arguments
function __fish_any_arg_in
    set -l haystack $argv
    for arg in (commandline -opc)
        contains -- $arg $haystack
        and return 0
    end
    return 1
end
