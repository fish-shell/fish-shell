complete -c cowsay -s e -d "Specify eye string" -r
complete -c cowsay -s f -d "Specify cow file" -x -a '(cowsay -l | tail -n +2 | string split " ")'
complete -c cowsay -s h -d "Display help and exit"
complete -c cowsay -s l -d "List all cowfiles"
complete -c cowsay -s n -d "No word wrapping"
complete -c cowsay -s T -d "Specify tongue string"
complete -c cowsay -s W -d "Column width" -r
complete -c cowsay -s b -d "Borg cow"
complete -c cowsay -s d -d "Dead cow"
complete -c cowsay -s g -d "Greedy cow"
complete -c cowsay -s p -d "Paranoid cow"
complete -c cowsay -s s -d "Stoned cow"
complete -c cowsay -s t -d "Tired cow"
complete -c cowsay -s w -d "Wired cow"
complete -c cowsay -s y -d "Young cow"
