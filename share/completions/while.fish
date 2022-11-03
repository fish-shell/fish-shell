# SPDX-FileCopyrightText: © 2019 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

complete -c while -s h -l help -d 'Display help and exit'
complete -c while -xa '(__fish_complete_subcommand)'
