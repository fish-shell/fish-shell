# SPDX-FileCopyrightText: © 2015 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

function __fish_print_debian_apache_confs
    # Helper script for completions for a2enconf/a2disconf
    for conf in /etc/apache2/conf-available/*.conf
        basename "$conf" .conf
    end
end
