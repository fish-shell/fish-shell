function __fish_complete_pids -d "Print a list of process identifiers along with brief descriptions"
    # This may be a bit slower, but it's nice - having the tty displayed is really handy
    # 'tail -n +2' deletes the first line, which contains the headers
    #  $fish_pid is removed from output by string match -r -v

    # Display the tty if available
    # But not if it's just question marks, meaning no tty
    __fish_ps -o pid,comm,tty | string match -r -v '^\s*'$fish_pid'\s' | tail -n +2 | string replace -r ' *([0-9]+) +([^ ].*[^ ]|[^ ]) +([^ ]+) *$' '$1\t$2 [$3]' | string replace -r ' *\[\?*\] *$' ''
end
