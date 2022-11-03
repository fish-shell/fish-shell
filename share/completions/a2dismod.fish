# SPDX-FileCopyrightText: © 2015 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

complete -c a2dismod -s q -l quiet -d "Don't show informative messages"
complete -c a2dismod -s p -l purge -d "Purge all traces of module"

complete -c a2dismod -xa '(__fish_print_debian_apache_mods)'
