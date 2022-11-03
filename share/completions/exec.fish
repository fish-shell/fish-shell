# SPDX-FileCopyrightText: © 2006 Axel Liljencrantz
#
# SPDX-License-Identifier: GPL-2.0-only

complete -c exec -n 'test (count (commandline -opc)) -eq 1' -s h -l help -d 'Display help and exit'
complete -c exec -xa "(__fish_complete_subcommand)"
