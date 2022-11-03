# SPDX-FileCopyrightText: © 2017 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

complete -c pkg_info -a '(__fish_print_pkg_add_packages)' -d Package
