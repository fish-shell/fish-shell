# On Solaris, `killall` kills all processes. So we don't want to bother providing completion
# support on that OS.
set -l OS (uname)
if test "$OS" = SunOS
    exit 0
end

__fish_make_completion_signals

for i in $__kill_signals
    set -l numname (string split " " -- $i)
    set -l number $numname[1]
    set -q numname[2]
    and set -l name $numname[2]
    complete -c killall -o $number -d $name
    complete -c killall -o $name
    # The `-s` flag doesn't work in OS X
    test "$OS" != Darwin
    and complete -c killall -s s -x -a "$number $name"
end

complete -c killall -xa '(__fish_complete_proc)'

if killall --version >/dev/null 2>/dev/null # GNU
    complete -c killall -s e -l exact -d 'Require an exact match for very long names'
    complete -c killall -s I -l ignore-case -d 'Do case insensitive process name match'
    complete -c killall -s g -l process-group -d 'Kill the process group to which the process belongs. The kill signal is only sent once per group, even if multiple processes belonging to the same process group were found'
    complete -c killall -s i -l interactive -d 'Interactively ask for confirmation before killing'
    complete -c killall -s u -l user -x -a "(__fish_complete_users)" -d 'Kill only processes the specified user owns. Command names are optional'
    complete -c killall -s w -l wait -d 'Wait for all killed processes to die'
    complete -c killall -s v -l version -d 'Print version'
else # probably BSD
    complete -c killall -s v -d 'Be more verbose about what will be done'
    complete -c killall -s e -d 'Use effective UID instead of the real UID for matching processes specified with the -u option'
    complete -c killall -s help -d 'Print help and exit'
    complete -c killall -s l -d 'List names of available signals and exit'
    complete -c killall -s m -d 'Case sensitive argument match for processed'
    complete -c killall -s s -d 'Simulate, but do not send any signals'
    complete -c killall -s d -d "Print detailed info. Doesn't send signals"
    complete -c killall -s u -x -a "(__fish_complete_users)" -d 'Kill only processes the specified user owns. Command names are optional'
    complete -c killall -s -u -l user -x -a "(__fish_complete_users)"
    complete -c killall -s t -d 'Limit to processes running on specified TTY'
    complete -c killall -s t -xa "(ps a -o tty | sed 1d | uniq)"
    complete -c killall -s c -x -d 'Limit to processes matching specified PROCNAME'
    complete -c killall -s z -d 'Do not skip zombies'
end
