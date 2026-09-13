set -l is_gnu
__fish_supports_version who; and set is_gnu --is-gnu

complete -c who -s m -d "Print hostname and user for stdin"

__fish_gnu_complete -c who -s b -l boot -d "Print time of last boot" $is_gnu
__fish_gnu_complete -c who -s d -l dead -d "Print dead processes" $is_gnu
__fish_gnu_complete -c who -s H -l heading -d "Print line of headings" $is_gnu
__fish_gnu_complete -c who -s l -l login -d "Print login process" $is_gnu
__fish_gnu_complete -c who -s p -l process -d "Print active processes spawned by init" $is_gnu
__fish_gnu_complete -c who -s q -l count -d "Print all login names and number of users logged on" $is_gnu
__fish_gnu_complete -c who -s r -l runlevel -d "Print current runlevel" $is_gnu
__fish_gnu_complete -c who -s s -l short -d "Print name, line, and time" $is_gnu
__fish_gnu_complete -c who -s t -l time -d "Print last system clock change" $is_gnu
__fish_gnu_complete -c who -s T -l mesg -d "Print users message status as +, - or ?" $is_gnu
__fish_gnu_complete -c who -s u -l users -d "List users logged in" $is_gnu

if test -n "$is_gnu"
    complete -c who -s a -l all -d "Same as -b -d --login -p -r -t -T -u"
    complete -c who -s i -l idle -d "Print idle time"
    complete -c who -l lookup -d "Canonicalize hostnames via DNS"
    complete -c who -s w -l writable -d "Print users message status as +, - or ?"
    complete -c who -l message -d "Print users message status as +, - or ?"
    complete -c who -l help -d "Display help and exit"
    complete -c who -l version -d "Display version and exit"
else
    complete -c who -s a -d "Same as -b -d -l -p -r -t -T -u"
end
