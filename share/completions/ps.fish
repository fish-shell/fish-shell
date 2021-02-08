# Completions for ps

set -l gnu_linux 0
if ps -V <&- >/dev/null 2>/dev/null
    set gnu_linux 1
end

# Separate out GNU/Linux-only options from BSD-compatible options
if test "$gnu_linux" -eq 1
    # Some short options are GNU-only
    complete -c ps -s a -d "Select all processes except session leaders and terminal-less"
    complete -c ps -s A -d "Select all"
    complete -c ps -s C -d "Select by command" -ra '(__fish_complete_list , __fish_complete_proc)'
    complete -c ps -s c -d 'Show different scheduler information for the -l option'
    complete -c ps -s d -d "Select all processes except session leaders"
    complete -c ps -s e -d "Select all"
    complete -c ps -s F -d "Extra full format"
    complete -c ps -s f -d "Full format"
    complete -c ps -s H -l forest -d "Show hierarchy"
    complete -c ps -s j -d "Jobs format"
    complete -c ps -s l -d "Long format"
    complete -c ps -s L -d "Show threads. With LWP/NLWP"
    complete -c ps -s M -d "Add column for security data"
    complete -c ps -s m -d 'Show threads after processes'
    complete -c ps -s N -d "Invert selection"
    complete -c ps -s n -d "Set namelist file" -r
    complete -c ps -s s -l sid -d "Select by session ID" -x -a "(__fish_complete_list , __fish_complete_pids)"
    complete -c ps -s T -d "Show threads. With SPID"
    complete -c ps -s u -l user -d "Select by user" -x -a "(__fish_complete_list , __fish_complete_users)"
    complete -c ps -s V -l version -d "Display version and exit"
    complete -c ps -s y -d "Do not show flags"

    # All long-only options are GNU extensions
    complete -c ps -l cols -l columns -l width -d "Set screen width" -r
    complete -c ps -l cumulative -d "Include dead child process data"
    complete -c ps -l deselect -d "Deselect all processes that do not fulfill conditions"
    complete -c ps -l headers -d "Repeat header lines, one per page"
    complete -c ps -l help -d "Display help and exit"
    complete -c ps -l info -d "Display debug info"
    complete -c ps -l lines -l rows -d "Set screen height" -r
    complete -c ps -l no-headers -d 'Print no headers'
    complete -c ps -l ppid -d "Select by parent PID" -x -a "(__fish_complete_list , __fish_complete_pids)"
    complete -c ps -l sort -d 'Specify sort order' -r
else
    # Assume BSD options otherwise
    complete -c ps -s a -d "Select processes for all users"
    complete -c ps -s c -d "Show only executable name, not full invocation"
    complete -c ps -s C -d "Use raw CPU calculation (ignore resident time)"
    complete -c ps -s d -d "Arrange in descending tree format"
    complete -c ps -s e -d "Show environment as well"
    complete -c ps -s f -d "Show command-line and environment info for swapped-out processes (root-only)"
    complete -c ps -s H -d "Show threads for all processes"
    complete -c ps -s h -d "Repeat headers on each page of output"
    complete -c ps -s j -d "BSD jobs format (user, pid, ppid, pgid, sid, jobc, state, tt, time, and cmd)"
    complete -c ps -s J -d "Select by jail (name or id)" -r
    complete -c ps -s l -d "BSD long format (uid, pid, ppid, cpu, pri, nice, vsz, rss, mwchan, state, tt, time, and cmd)"
    complete -c ps -s L -d "List available columns for -O/-o"
    complete -c ps -s M -d "Extract names from specified core file" -r
    complete -c ps -s m -d "Sort by memory usage"
    complete -c ps -s N -d "Extract names from specified system/installation" -r
    complete -c ps -s r -d "Sort by CPU usage"
    complete -c ps -s S -d "Include sum of child exited children in parent process times"
    complete -c ps -s T -d "Select by tty connected to named stdin" -r
    complete -c ps -s t -d "Select by tty" -r
    complete -c ps -s u -d "BSD user format (user, pid, %cpu, %mem, vsz, rss, tt, state, start, time, and cmd)"
    complete -c ps -s v -d "BSD v format (pid, state, time, sl, re, pagein, vsz, rss, lim, tsiz, %cpu, %mem)"
    complete -c ps -s X -d "When filtering, skip processes without controlling terminal (default)"
    complete -c ps -s x -d "When filtering, include processes without controlling terminal"
end

# Shared options
complete -c ps -s O -d "User defined format" -x
complete -c ps -s w -d "Wide output"

# Shared options (but long opt not available on BSD)
set -l bsd_null ""
if test $gnu_linux -eq 0
    set bsd_null
end

complete -c ps -s o -lformat$bsd_null -d "User defined format" -x
complete -c ps -s Z -lcontext$bsd_null -d "Include security info"
complete -c ps -s t -ltty$bsd_null -d "Select by tty" -r
complete -c ps -s G -lgroup$bsd_null -d "Select by group" -x -a "(__fish_complete_list , __fish_complete_groups)"
complete -c ps -s U -luser$bsd_null -d "Select by user" -x -a "(__fish_complete_list , __fish_complete_users)"
complete -c ps -s p -lpid$bsd_null -d "Select by PID" -x -a "(__fish_complete_list , __fish_complete_pids)"
