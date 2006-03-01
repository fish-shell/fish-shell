

complete -c jobs -s p -l pid -d (N_ "Show the process id of each process in the job")
complete -c jobs -s g -l group -d (N_ "Show group id of job")
complete -c jobs -s c -l command -d (N_ "Show commandname of each job")
complete -c jobs -s l -l last -d (N_ "Only show status for last job to be started")
