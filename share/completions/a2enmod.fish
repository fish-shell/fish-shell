complete -c a2enmod -s q -l quiet -d "Don't show informative messages"

complete -c a2enmod -xa '(__fish_print_debian_apache_mods)'
