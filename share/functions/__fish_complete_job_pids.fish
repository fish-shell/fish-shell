function __fish_complete_job_pids --description "Print a list of job PIDs and their commands"
    if set -l jobpids (jobs -p)
        # when run at the commandline, the first line of output is a header, but
        # that doesn't seem to be printed when you run jobs in a subshell

        # then we can use the jobs command again to get the corresponding
        # command to provide as a description for each job PID
        for jobpid in $jobpids
            set -l cmd (jobs -c $jobpid)
            printf "%s\tJob: %s\n" $jobpid $cmd
        end
    end
end
