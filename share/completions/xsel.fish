
# magic completion safety check (do not remove this comment)
if not type -q xsel
    exit
end
complete -c xsel -s a -l append -d "Append input to selection"
complete -c xsel -s f -l follow -d "Append to selection as input grows"
complete -c xsel -s i -l input -d "Read into selection"
complete -c xsel -s o -l output -d "Write selection"
complete -c xsel -s c -l clear -d "Clear selection"
complete -c xsel -s d -l delete -d "Delete selection"
complete -c xsel -s p -l primary -d "Use primary selection"
complete -c xsel -s s -l secondary -d "Use secondary selection"
complete -c xsel -s b -l clipboard -d "Use clipboard selection"
complete -c xsel -s k -l keep -d "Make current selections persistent after program exit"
complete -c xsel -s x -l exchange -d "Exchange primary and secondary selections"
complete -c xsel -l display -x -d "X server display"
complete -c xsel -s t -l selectionTimeout -d "Timeout for retrieving selection"
complete -c xsel -s l -l logfile -f -d "Error log"
complete -c xsel -s n -l nodetach -d "Do not detach from the controlling terminal"
complete -c xsel -s h -l help -d "Display help and exit"
complete -c xsel -s v -l verbose -d "Print informative messages"
complete -c xsel -l version -d "Display version and exit"
