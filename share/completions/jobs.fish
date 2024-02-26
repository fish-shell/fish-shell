complete -c jobs -s h -l help -d 'Display help and exit'
complete -c jobs -s p -l pid -d "Show the process id of each process in the job"
complete -c jobs -s g -l group -d "Show group id of job"
complete -c jobs -s c -l command -d "Show commandname of each job"
complete -c jobs -s l -l last -d "Only show status for last job to be started"
complete -c jobs -s q -l quiet -l query -d "Check if a job exists without output"
