complete -c cowthink -s e -d "Specify eye string" -r
complete -c cowthink -s f -d "Specify cow file" -x -a '(cowthink -l | tail -n +2 | string split " ")'
complete -c cowthink -s h -d "Display help and exit"
complete -c cowthink -s l -d "List all cowfiles"
complete -c cowthink -s n -d "No word wrapping"
complete -c cowthink -s T -d "Specify tongue string"
complete -c cowthink -s W -d "Column width" -r
complete -c cowthink -s b -d "Borg cow"
complete -c cowthink -s d -d "Dead cow"
complete -c cowthink -s g -d "Greedy cow"
complete -c cowthink -s p -d "Paranoid cow"
complete -c cowthink -s s -d "Stoned cow"
complete -c cowthink -s t -d "Tired cow"
complete -c cowthink -s w -d "Wired cow"
complete -c cowthink -s y -d "Young cow"
