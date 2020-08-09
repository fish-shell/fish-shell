complete -c a2dissite -s q -l quiet -d "Don't show informative messages"
complete -c a2dissite -s p -l purge -d "Purge all traces of module"

complete -c a2dissite -xa '(__fish_print_debian_apache_sites)'
