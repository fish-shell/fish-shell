# calendar

complete -c calendar -s A -s l -d "Today + N days forward"
complete -c calendar -s a -d "Process all users' calendars and mail the results"
complete -c calendar -s B -d "Today - N days backward"
complete -c calendar -s b -d "Cyrillic date calculation mode"
complete -c calendar -s e -d "Today + N days forward, if today is Friday"
complete -c calendar -s f -rF -d "Path to default calendar file"
complete -c calendar -s t -x -d "Today is [[[cc]yy]mm]dd]"
complete -c calendar -s w -d "Print day of the week name"
