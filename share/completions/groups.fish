# SPDX-FileCopyrightText: © 2020 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

complete -c groups -x -a "(__fish_complete_users)"
complete -c groups -l help -d 'Display help message'
complete -c groups -l version -d 'Display version information'
