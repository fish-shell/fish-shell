if tail --version > /dev/null ^ /dev/null
    complete -c tail -s c -l bytes -x -d 'output the last K bytes; alternatively, use -c +K to output bytes starting with the Kth of each file'
    complete -c tail -s f -l follow -xa 'name descriptor' -d 'output appended data as the file grows; -f -l follow, and --follow=descriptor are equivalent'
    complete -c tail -s F -d 'same as --follow=name --retry'
    complete -c tail -s n -l lines -x -d 'output the last K lines, instead of the last 10; or use -n +K to output lines starting with the Kth'
    complete -c tail -l max-unchanged-stats -x -d 'with --follow=name, reopen a FILE which has not changed size after N iterations'
    complete -c tail -l pid -d 'with -f, terminate after process ID, PID dies' -xa '(__fish_complete_pids)'
    complete -c tail -s q -l quiet -l silent -d 'never output headers giving file names'
    complete -c tail -l retry -d 'keep trying to open a file even when it is or becomes inaccessible; useful when following by name, i.e., with --follow=name'
    complete -c tail -s s -l sleep-interval -x -d 'with -f, sleep for approximately N seconds (default 1.0) between iterations'
    complete -c tail -s v -l verbose -d 'always output headers giving file names'
    complete -c tail -l help -d 'display this help and exit'
    complete -c tail -l version -d 'output version information and exit'
else # OSX and similar - no longopts (and fewer shortopts)
    complete -c tail -s b -x -d 'output last K 512 byte blocks'
    complete -c tail -s c -x -d 'output the last K bytes or only K bytes with -r'
    complete -c tail -s f -d 'output appended data as the file grows'
    complete -c tail -s F -d 'Like -f, but also follow renamed or rotated files'
    complete -c tail -s n -x -d 'output the last K lines, instead of the last 10 - or only K lines with -r'
    complete -c tail -s q -d 'never output headers giving file names'
	complete -c tail -s r -d 'Display input in reverse order' # Only in OSX
end
