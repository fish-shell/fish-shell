# SPDX-FileCopyrightText: © 2019 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

complete -c begin -s h -l help -d 'Display help and exit'
complete -c begin -xa '(__fish_complete_subcommand)'
