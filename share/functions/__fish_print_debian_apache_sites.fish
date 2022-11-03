# SPDX-FileCopyrightText: © 2015 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

function __fish_print_debian_apache_sites
    # Helper script for completions for a2ensite/a2dissite
    for site in /etc/apache2/sites-available/*
        basename "$site" .conf
    end
end
