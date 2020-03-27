# gitk - The Git repository browser

source $__fish_data_dir/completions/git.fish

complete -c gitk -f -n 'not contains -- -- (commandline -opc)' -a '(__fish_git_ranges)'
complete -c gitk -n 'contains -- -- (commandline -opc)' -F

complete -c gitk -l all -d 'Show all refs (branches, tags, etc.)'
complete -c gitk -l since=YYYY-MM-DD -x -d 'Show commits more recent that a specific date'
complete -c gitk -l until=YYYY-MM-DD -x -d 'Show commits older than a specific date'
complete -c gitk -l date-order -d 'Sort commits by date when possible'
complete -c gitk -l merge -d 'On a merge conflict, show commits that modify conflicting files'
complete -c gitk -l left-right -d 'Mark which side of a symmetric difference a commit is reachable from'
complete -c gitk -l full-history -d 'When filtering history with -- path..., do not prune some history'
complete -c gitk -l simplify-merges -d 'Hide needless merges from history'
complete -c gitk -l ancestry-path -d 'Only display commits that exist directly on the ancestry chain between the given range'
complete -c gitk -l argscmd= -d 'Command to be run to determine th revision range to show'
complete -c gitk -l select-commit= -d 'Select the specified commit after loading the graph, instead of HEAD'
complete -c gitk -l select-commit=HEAD -d 'Select the specified commit after loading the graph, instead of HEAD'
complete -c gitk -n 'string match -rq -- "^--select-commit=" (commandline -ct)' -xa '(printf -- "--select-commit=%s\n" (__fish_git_refs))'
complete -c gitk -s n -l max-count -x -d 'Limit the number of commits to output'
complete -c gitk -xa -L1 -d '-L<start>,<end>:<file> trace the evolution of a line range'
complete -c gitk -xa -L. -d '-L<funcname>:<file> trace the evolution of a function name regex'
complete -c gitk -n 'string match -rq -- "^-L[^:]*": (commandline -ct)' -xa '(
    set -l tok (string split -m 1 -- : (commandline -ct))
    printf -- "$tok[1]:%s\n" (complete -C": $tok[2]")
)'
