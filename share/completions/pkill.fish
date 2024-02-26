__fish_complete_pgrep pkill
__fish_make_completion_signals
for i in $__kill_signals
    echo $i | read -l number name
    complete -c pkill -o $number -d $name
    complete -c pkill -o $name -d $name
end
