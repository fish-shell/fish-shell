
# magic completion safety check (do not remove this comment)
if not type -q pkill
    exit
end

__fish_complete_pgrep pkill
__fish_make_completion_signals
for i in $__kill_signals
    echo $i | read number name
    complete -c pkill -o $number -d $name
    complete -c pkill -o $name -d $name
end
