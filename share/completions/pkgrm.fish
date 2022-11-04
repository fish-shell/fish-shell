# SPDX-FileCopyrightText: © 2007 Axel Liljencrantz
# SPDX-FileCopyrightText: © 2009 fish-shell contributors
# SPDX-FileCopyrightText: © 2022 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

#completion for pkgrm

complete -f -c pkgrm -a '(__fish_crux_packages)' -d 'Package to remove'

complete -c pkgrm -o r -l root -d 'Alternative installation root'
complete -f -c pkgrm -o v -l version -d 'Print version'
complete -f -c pkgrm -o h -l help -d 'Print help'
