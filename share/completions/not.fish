# SPDX-FileCopyrightText: © 2006 Axel Liljencrantz
#
# SPDX-License-Identifier: GPL-2.0-only

complete -c not -s h -l help -d 'Display help and exit'
complete -c not -xa '(__fish_complete_subcommand)'
