
# magic completion safety check (do not remove this comment)
if not type -q a2enmod
    exit
end
complete -c a2enmod -s q -l quiet -d "Don't show informative messages"

complete -c a2enmod -xa '(__fish_print_debian_apache_mods)'

