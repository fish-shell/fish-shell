

complete -c jobs -s p -l pid -d (_ "Show the process id of each process in the job")
complete -c jobs -s g -l group -d (_ "Show group id of job")
complete -c jobs -s c -l command -d (_ "Show commandname of each job")
complete -c jobs -s l -l last -d (_ "Only show status for last job to be started")
