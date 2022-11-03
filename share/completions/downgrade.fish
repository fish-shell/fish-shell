# SPDX-FileCopyrightText: © 2020 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

# Arch Linux package downgrader tool
complete -c downgrade -f
complete -c downgrade -xa "(__fish_print_pacman_packages --installed)"
