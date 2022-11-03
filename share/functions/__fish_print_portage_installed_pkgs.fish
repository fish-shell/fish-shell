# SPDX-FileCopyrightText: © 2018 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

function __fish_print_portage_installed_pkgs --description 'Print all installed packages (non-deduplicated)'
    find /var/db/pkg -mindepth 2 -maxdepth 2 -type d -printf '%P\n' | string replace -r -- '-[0-9][0-9.]*.*$' ''
end
