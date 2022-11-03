# SPDX-FileCopyrightText: © 2018 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

# determine if this is the very first argument (regardless if switch or not)
function __fish_is_first_arg
    set -l tokens (commandline -poc)
    test (count $tokens) -eq 1
end
