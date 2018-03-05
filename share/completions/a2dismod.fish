
# magic completion safety check (do not remove this comment)
if not type -q a2dismod
    exit
end
complete -c a2dismod -s q -l quiet -d "Don't show informative messages"
complete -c a2dismod -s p -l purge -d "Purge all traces of module"

complete -c a2dismod -xa '(__fish_print_debian_apache_mods)'

