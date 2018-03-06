
# magic completion safety check (do not remove this comment)
if not type -q a2ensite
    exit
end
complete -c a2ensite -s q -l quiet -d "Don't show informative messages"

complete -c a2ensite -xa '(__fish_print_debian_apache_sites)'

