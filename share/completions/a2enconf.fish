
# magic completion safety check (do not remove this comment)
if not type -q a2enconf
    exit
end
complete -c a2enconf -s q -l quiet -d "Don't show informative messages"

complete -c a2enconf -xa '(__fish_print_debian_apache_confs)'

