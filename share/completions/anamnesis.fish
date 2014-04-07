
complete -c anamnesis -l version -d "Show program's version number and exit"
complete -c anamnesis -s h -l help -d "Show a help message and exit"
complete -c anamnesis -l start -d "Starts anamnesis daemon"
complete -c anamnesis -l stop -d "Stops anamnesis daemon"
complete -c anamnesis -l restart -d "Restarts anamnesis daemon"
complete -c anamnesis -s b -l browser -d "Opens anamnesis browser with clipboard history"
complete -c anamnesis -l cleanup -d "Performs a cleanup on the clipboard database"
complete -c anamnesis -s l -l list -d "Prints the clipboard history last N values"
complete -c anamnesis -l filter -d "Use keywords to filter the clips to be listed"
complete -c anamnesis -s a -l add -d "Adds a value to the clipboard"
complete -c anamnesis -l remove -d "Removes the clipboard element with the given id"
complete -c anamnesis -l brief -d "Print only a brief version of long clipboard elements"

