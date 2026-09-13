# Completions for top
complete -c top -s U -d "Monitor user" -x -a "(__fish_complete_users)"

if __fish_supports_version top
    complete -c top -s b -d "Batch mode"
    complete -c top -s c -d "Toggle command line/program name"
    complete -c top -s d -d "Update interval" -x
    complete -c top -s h -d "Display help and exit"
    complete -c top -s i -d "Toggle idle processes"
    complete -c top -s n -d "Maximum iterations" -x
    complete -c top -s u -d "Monitor effective UID" -x -a "(__fish_complete_users)"
    complete -c top -s p -d "Monitor PID" -x -a "(__fish_complete_pids)"
    complete -c top -s s -d "Secure mode"
    complete -c top -s S -d "Cumulative mode"
    complete -c top -s v -d "Display version and exit"
else
    complete -c top -s a -d "Use accumulative event counting mode"
    complete -c top -s d -d "Use delta event counting mode"
    complete -c top -s e -d "Use absolute event counting mode"
    complete -c top -s c -x -d "Set event counting mode"
    complete -c top -s F -d "Do not calculate shared-library statistics"
    complete -c top -s f -d "Calculate shared-library statistics"
    complete -c top -s h -d "Display help"
    complete -c top -s i -x -d "Update shared-library statistics every N samples"
    complete -c top -s l -x -d "Use logging mode with samples"
    complete -c top -o ncols -x -d "Number of columns"
    complete -c top -s o -x -d "Sort by key"
    complete -c top -s O -x -d "Sort by secondary key"
    complete -c top -s R -d "Do not traverse memory objects"
    complete -c top -s r -d "Traverse memory objects"
    complete -c top -s S -d "Display swap usage"
    complete -c top -s s -x -d "Set delay between updates"
    complete -c top -s n -x -d "Number of processes"
    complete -c top -o stats -x -d "Display process statistics"
    complete -c top -o pid -x -a "(__fish_complete_pids)" -d "Monitor process ID"
    complete -c top -o user -x -a "(__fish_complete_users)" -d "Monitor user"
    complete -c top -s u -d "Sort by CPU usage, then CPU time"
end
