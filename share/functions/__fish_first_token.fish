# SPDX-FileCopyrightText: © 2018 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

# Retrieves the first non-switch argument from the command line buffer
function __fish_first_token
    set -l tokens (commandline -co)
    set -e tokens[1]
    set -l tokens (string replace -r --filter '^([^-].*)' '$1' -- $tokens)
    if set -q tokens[1]
        echo $tokens[1]
    end
end
