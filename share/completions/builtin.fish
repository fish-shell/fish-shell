# SPDX-FileCopyrightText: © 2006 Axel Liljencrantz
#
# SPDX-License-Identifier: GPL-2.0-only

complete -c builtin -n 'test (count (commandline -opc)) -eq 1' -s h -l help -d 'Display help and exit'
complete -c builtin -n 'test (count (commandline -opc)) -eq 1' -s n -l names -d 'Print names of all existing builtins'
complete -c builtin -n 'test (count (commandline -opc)) -eq 1' -s q -l query -d 'Tests if builtin exists'
complete -c builtin -n 'test (count (commandline -opc)) -eq 1' -xa '(builtin -n)'
complete -c builtin -n 'test (count (commandline -opc)) -ge 2' -xa '(__fish_complete_subcommand)'
