# SPDX-FileCopyrightText: © 2012 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

complete -c source -k -xa '(__fish_complete_suffix .fish)'
complete -c source -s h -l help -d 'Display help and exit'
