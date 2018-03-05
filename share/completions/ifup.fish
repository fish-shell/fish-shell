
# magic completion safety check (do not remove this comment)
if not type -q ifup
    exit
end
complete -x -c ifup -a "(__fish_print_interfaces)" -d "Network interface"
complete -c ifup -l force  -d "force"
