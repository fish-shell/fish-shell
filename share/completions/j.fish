# SPDX-FileCopyrightText: © 2017 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

function __history_completions --argument-names limit
    if string match -q "" -- "$limit"
        set limit 25
    end

    set -l tokens (commandline --current-process --tokenize)
    history --prefix (commandline) | string replace -r \^$tokens[1]\\s\* "" | head -n$limit
end

# erase the stock autojump completions, which are no longer needed with this
complete -c j -e
complete -k -c j -a '(__history_completions 25)' -f
