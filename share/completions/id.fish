complete id -xa "(__fish_complete_users)"

if string match -eq 'GNU coreutils' (id --version 2>&1)
    complete id -s Z -l context -d "Print security context"
    complete id -s z -l zero -d "Delimit entries with NUL"
    complete id -s n -l name -d "Print name, not number"
    complete id -s g -l group -d "Print effective group id"
    complete id -s G -l groups -d "Print all group ids"
    complete id -s r -l real -d "Print real ID, not effective"
    complete id -s u -l user -d "Print effective user ID"
    complete id -l help -d "Display help and exit"
    complete id -l version -d "Display version and exit"
else
    # macOS id shell_cmds-240.100.15
    #      id [user]
    #      id -A
    #      id -F [user]
    #      id -G [-n] [user]
    #      id -P [user]
    #      id -g [-nr] [user]
    #      id -p [user]
    #      id -u [-nr] [user]
    complete id -s A -d "Print process audit ID"
    complete id -s F -d "Print full name of the user"
    complete id -s G -d "Print all group ids"
    complete id -s P -d "Print as passwd file entry"
    complete id -s g -d "Print effective group id"
    complete id -s n -d "Print name, not number"
    complete id -s p -d "Human-readable output"
    complete id -s r -d "Print real ID, not effective"
    complete id -s u -d "Print effective user ID"
end
