# SPDX-FileCopyrightText: © 2019 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

complete -c for -n 'test (count (commandline -opc)) -eq 1' -s h -l help -d 'Display help and exit'
complete -c for -n 'test (count (commandline -opc)) -eq 1' -f
complete -c for -n 'test (count (commandline -opc)) -eq 2' -xa in
