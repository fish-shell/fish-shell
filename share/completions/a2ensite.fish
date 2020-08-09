complete -c a2ensite -s q -l quiet -d "Don't show informative messages"

complete -c a2ensite -xa '(__fish_print_debian_apache_sites)'
