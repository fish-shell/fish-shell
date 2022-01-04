complete -c id -xa "(__fish_complete_users)"

if string match -eq 'GNU coreutils' (id --version 2>&1)
    complete -c id -s Z -l context -d "Print security context"
    complete -c id -s z -l zero -d "Delimit entries with NUL"
end

complete -c id -s g -l group -d "Print effective group id"
complete -c id -s G -l groups -d "Print all group ids"
complete -c id -s n -l name -d "Print name, not number"
complete -c id -s r -l real -d "Print real ID, not effective"
complete -c id -s u -l user -d "Print effective user ID"
complete -c id -l help -d "Display help and exit"
complete -c id -l version -d "Display version and exit"
