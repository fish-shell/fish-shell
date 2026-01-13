__fish_complete_pgrep pgrep

if pgrep --help &>/dev/null
    complete -c pgrep -s d -l delimiter -r -d 'Output delimiter'
    complete -c pgrep -s l -l list-name -d 'List the process name as well as the process ID'
    complete -c pgrep -s a -l list-full -d 'List the full command line as well as the process ID'
    complete -c pgrep -s Q -l shell-quote -d 'Output the command line in shell-quoted form'
    complete -c pgrep -l quiet -d 'Do not write anything to standard output'
else # non GNU
    complete -c pgrep -s d -r -d 'Output delimiter'
    complete -c pgrep -s l -d 'List the process name as well as the process ID'
end
