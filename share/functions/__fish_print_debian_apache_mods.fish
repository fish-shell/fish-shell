# SPDX-FileCopyrightText: © 2015 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

function __fish_print_debian_apache_mods
    # Helper script for completions for a2enmod/a2dismod
    for mod in /etc/apache2/mods-available/*.load
        basename "$mod" .load
    end
end
