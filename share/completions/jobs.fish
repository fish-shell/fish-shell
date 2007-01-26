

complete -c jobs -s h -l help --description 'Display help and exit'
complete -c jobs -s p -l pid --description "Show the process id of each process in the job"
complete -c jobs -s g -l group --description "Show group id of job"
complete -c jobs -s c -l command --description "Show commandname of each job"
complete -c jobs -s l -l last --description "Only show status for last job to be started"
