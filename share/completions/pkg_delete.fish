# SPDX-FileCopyrightText: © 2017 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

complete -c pkg_delete -a '(__fish_print_pkg_add_packages)' -d Package
complete -c pkg_delete -o a -d 'Delete unused deps'
complete -c pkg_delete -o V -d 'Turn on stats'
