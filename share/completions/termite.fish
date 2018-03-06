
# magic completion safety check (do not remove this comment)
if not type -q termite
    exit
end
complete -c termite -s h -l help -d "Display help message"
complete -c termite -s v -l version -d "Display version information"
complete -r -c termite -s e -l exec -d "Tell termite start <cmd> instead of the shell"
complete -x -c termite -s r -l role -d "The role to set the termite window to report itself with"
complete -x -c termite -s t -l title -d "Set the termite window's title"
complete -r -c termite -s d -l directory -d "Tell termite to change to <dir> when launching"
complete -x -c termite -l geometry -d "Override the window geometry in pixels"
complete -c termite -l hold -d "Keep termite open after the child process exits"
complete -x -c termite -l display -d "Launch on <disp> X display"
complete -r -c termite -s c -l config -d "Config file to use"
complete -x -c termite -l name -d "Set the windows name part of WM_CLASS property"
complete -x -c termite -l class -d "Set the windows class part of the WM_CLASS property"
