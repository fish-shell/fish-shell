complete -c a2disconf -s q -l quiet -d "Don't show informative messages"
complete -c a2disconf -s p -l purge -d "Purge all traces of module"

complete -c a2disconf -xa '(__fish_print_debian_apache_confs)'
