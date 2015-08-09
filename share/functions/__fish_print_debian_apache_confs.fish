function __fish_print_debian_apache_confs
    # Helper script for completions for a2enconf/a2disconf
    for conf in /etc/apache2/conf-available/*.conf
        basename "$conf" .conf
    end
end
