# SPDX-FileCopyrightText: © 2013 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

function __fish_complete_cabal
    set -l cmd (commandline -poc)
    if test (count $cmd) -gt 1
        cabal $cmd[2..-1] --list-options
    else
        cabal --list-options
    end
end

complete -c cabal -a '(__fish_complete_cabal)'
