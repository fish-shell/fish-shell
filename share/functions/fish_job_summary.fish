function fish_job_summary -a job_id is_foreground cmd_line signal_or_end_name signal_desc proc_pid proc_name
    # job_id: ID of the job that stopped/terminated/ended.
    # is_foreground: 1 if the job was running in the foreground, 0 otherwise.
    # cmd_line: The command line of the job.
    # signal_or_end_name: If terminated by signal, the name of the signal (e.g. SIGTERM).
    #   If ended, the string "ENDED". If stopped, the string "STOPPED".
    # signal_desc: A description of the signal (e.g. "Polite quite request").
    #   Not provided if the job stopped or ended without a signal.
    # If the job has more than one process:
    # proc_pid: the pid of the process affected.
    # proc_name: the name of that process.
    # If the job has only one process, these two arguments will not be provided.

    # Print nothing if we get SIGINT in the foreground process group, to avoid spamming
    # obvious stuff on the console (#1119). If we get SIGINT for the foreground
    # process, assume the user typed ^C and can see it working. It's possible they
    # didn't, and the signal was delivered via pkill, etc., but the SIGINT/SIGTERM
    # distinction is precisely to allow INT to be from a UI
    # and TERM to be programmatic, so this assumption is keeping with the design of
    # signals. If echoctl is on, then the terminal will have written ^C to the console.
    # If off, it won't have. We don't echo ^C either way, so as to respect the user's
    # preference.
    if test $signal_or_end_name = "SIGINT"; and test $is_foreground -eq 1
        return
    end

    set -l ellipsis '...'
    if string match -iqr 'utf.?8' -- $LANG
        set ellipsis \u2026
    end

    set -l max_cmd_len 32
    if test (string length $cmd_line) -gt $max_cmd_len
        set -l truncated_len (math $max_cmd_len - (string length $ellipsis))
        set cmd_line (string trim (string sub -l $truncated_len $cmd_line))$ellipsis
    end

    switch $signal_or_end_name
        case "STOPPED"
            printf ( _ "fish: Job %s, '%s' has stopped\n" ) $job_id $cmd_line
        case "ENDED"
            printf ( _ "fish: Job %s, '%s' has ended\n" ) $job_id $cmd_line
        case 'SIG*'
            if test -n "$proc_pid"
                printf ( _ "fish: Process %s, '%s' from job %s, '%s' terminated by signal %s (%s)\n" ) \
                    $proc_pid $proc_name $job_id $cmd_line $signal_or_end_name $signal_desc
            else
                printf ( _ "fish: Job %s, '%s' terminated by signal %s (%s)\n" ) \
                    $job_id $cmd_line $signal_or_end_name $signal_desc
            end
    end >&2
end
