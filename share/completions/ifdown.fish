
# magic completion safety check (do not remove this comment)
if not type -q ifdown
    exit
end
complete -x -c ifdown -a "(__fish_print_interfaces)" -d "Network interface"
complete -c ifdown -l force -d "force"
