
__fish_complete_pgrep pkill
__fish_make_completion_signals
for i in $__kill_signals
    set number (echo $i | cut -d " " -f 1)
    set name (echo $i | cut -d " " -f 2)
    complete -c pkill -o $number -d $name
    complete -c pkill -o $name -d $name
end
