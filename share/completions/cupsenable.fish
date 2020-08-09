__fish_complete_lpr cupsenable

complete -c cupsenable -s c -d 'Cancels all jobs on the named destination'
complete -c cupsenable -l hold -d 'Holds remaining jobs on the named printer'
complete -c cupsenable -l release -d 'Releases pending jobs for printing'
complete -c cupsenable -s r -d 'Cancel reason' -x
