# SPDX-FileCopyrightText: © 2018 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

function __fish_print_portage_available_pkgs --description 'Print all available packages'
    find (__fish_print_portage_repository_paths) -mindepth 2 -maxdepth 2 -type d ! '(' '(' -path '*/eclass/*' -o -path '*/metadata/*' -o -path '*/profiles/*' -o -path '*/.*/*' ')' -prune ')' -printf '%P\n'
end
