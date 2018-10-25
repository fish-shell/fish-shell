
function __fish_complete_pgrep -d 'Complete pgrep/pkill' --argument-names cmd
    # Some implementations only track the first X characters of a process name
    # (with linux having the lowest X = 15).
    # Just cut at 15 for all, and put the rest in the description.
    complete -c $cmd -xa '(__fish_complete_proc | string replace -r "(.{15})(.+)" "\$1\\t\$2")'
    complete -c $cmd -s f -d 'Match pattern against full command line'
    complete -c $cmd -s g -d 'Only match processes in the process group' -xa '(__fish_complete_list , __fish_complete_groups)'
    complete -c $cmd -s G -d "Only match processes whose real group ID is listed. Group 0 is translated into $cmd\'s own process group" -xa '(__fish_complete_list , __fish_complete_groups)'
    complete -c $cmd -s n -d 'Select only the newest process'
    complete -c $cmd -s o -d 'Select only the oldest process'
    complete -c $cmd -s P -d 'Only match processes whose parent process ID is listed' -xa '(__fish_complete_list , __fish_complete_pids)'
    complete -c $cmd -s s -d "Only  match  processes  whose  process  session  ID  is listed.  Session ID 0 is translated into $cmd\'s own session ID."
    complete -c $cmd -s t -d 'Only match processes whose controlling terminal is listed.  The terminal name should  be  specified without the "/dev/" prefix' -r
    complete -c $cmd -s u -d 'Only  match  processes  whose  effective  user ID is listed' -xa '(__fish_complete_list , __fish_complete_users)'
    complete -c $cmd -s U -d 'Only match processes whose real user ID is listed' -xa '(__fish_complete_list , __fish_complete_users)'
    complete -c $cmd -s v -d 'Negates the matching'
    complete -c $cmd -s x -d ' Only match processes whose name (or command line if -f is specified) exactly match the pattern'
end
