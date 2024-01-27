function __schtasks_print_tasks -d 'Helper function to print tasks'
    schtasks /query /fo csv /nh | awk -F ',' '{ print $1 }'
end

function __schtasks_change_complete_args -a previous_token
    if test "$previous_token" = /tn
        __schtasks_print_tasks
        return
    end

    if string match -r -q -- "$previous_token" '^/r?u$'
        __fish_print_windows_users
        return
    end

    if __fish_seen_argument -w s
        echo -e '/u\tRun this command with the permissions of the specified user account'
    end

    if __fish_seen_argument -w u
        echo -e '/p\tSpecify the password of the user account specified in the /u parameter'
    end

    if not __fish_seen_argument -w et -w du
        echo -e '/et\tSpecify the end time for the task
/du\tA value that specifies the duration'
    end

    if not __fish_seen_argument -w ENABLE -w DISABLE
        echo -e '/ENABLE\tSpecify to enable the scheduled task
/DISABLE\tSpecify to disable the scheduled task'
    end

    echo -e '/tn\tIdentify the task to be changed
/s\tSpecify the name or IP address of a remote computer
/ru\tChange the user name under which the scheduled task has to run
/rp\tSpecify a new password for the existing user account, or the user account specified by /ru
/tr\tChange the program that the task runs
/st\tSpecify the start time for the task
/ri\tSpecify the repetition interval for the scheduled task
/k\tStop the program that the task runs at the time specified by /et or /du
/sd\tSpecify the first date on which the task should be run
/ed\tSpecify the last date on which the task should be run
/it\tSpecify to run the scheduled task only when the run as user is logged on to the computer
/z\tSpecify to delete the task upon the completion of its schedule
/?\tShow help'
end

function __schtasks_create_complete_args -a previous_token
    if test "$previous_token" = /sc
        echo -e 'MINUTE\tSpecify the number of minutes before the task should run
HOURLY\tSpecify the number of hours before the task should run
DAILY\tSpecify the number of days before the task should run
WEEKLY\tSpecify the number of weeks before the task should run
MONTHLY\tSpecify the number of months before the task should run
ONCE\tSpecify that that task runs once at a specified date and time
ONSTART\tSpecify that the task runs every time the system starts
ONLOGON\tSpecify that the task runs whenever a user logs on
ONIDLE\tSpecify that the task runs whenever the system is idle for a specified period of time'
        return
    end

    if string match -r -q -- "$previous_token" '^/r?u$'
        __fish_print_windows_users
        return
    end

    if test "$previous_token" = /mo
        echo -e 'MINUTE\tSpecify that the task runs every n minutes
HOURLY\tSpecify that the task runs every n hours
DAILY\tSpecify that the task runs every n days
WEEKLY\tSpecify that the task runs every n weeks
MONTHLY\tSpecify that the task runs every n months
ONCE\tSpecify that the task runs once
ONSTART\tSpecify that the task runs at startup
ONLOGON\tSpecify that the task runs when the user specified by the /u parameter logs on
ONIDLE\tSpecify that the task runs after the system is idle for the number of minutes specified by /i'
        return
    end

    if __fish_seen_argument -w s
        echo -e '/u\tRun this command with the permissions of the specified user account'
    end

    if __fish_seen_argument -w u
        echo -e '/p\tSpecify the password of the user account specified in the /u parameter'
    end

    if not __fish_seen_argument -w et -w du
        echo -e '/et\tSpecify the time of day that a minute or hourly task schedule ends
/du\tSpecify a maximum length of time for a minute or hourly schedule'
    end

    echo -e '/sc\tSpecify the schedule type
/tn\tSpecify a name for the task
/tr\tSpecify the program or command that the task runs
/s\tSpecify the name or IP address of a remote computer
/ru\tRun the task with permissions of the specified user account
/rp\tSpecify a the password for the existing user account, or the user account specified by /ru
/mo\tSpecify how often the task runs within its schedule type
/d\tSpecify how often the task runs within its schedule type
/m\tSpecify a month or months of the year during which the scheduled task should run
/i\tSpecify how many minutes the computer is idle before the task starts
/st\tSpecify the start time for the task
/ri\tSpecify the repetition interval for the scheduled task
/k\tStop the program that the task runs at the time specified by /et or /du
/sd\tSpecify the date on which the task schedule starts
/ed\tSpecify the date on which the schedule ends
/it\tSpecify to run the scheduled task only when the run as user is logged on to the computer
/z\tSpecify to delete the task upon the completion of its schedule
/f\tSpecify to create the task and suppress warnings if the specified task already exists
/?\tShow help'
end

function __schtasks_delete_complete_args -a previous_token
    if test "$previous_token" = /tn
        __schtasks_print_tasks
        return
    end

    if test "$previous_token" = /u
        __fish_print_windows_users
        return
    end

    if __fish_seen_argument -w s
        echo -e '/u\tRun this command with the permissions of the specified user account'
    end

    if __fish_seen_argument -w u
        echo -e '/p\tSpecify the password of the user account specified in the /u parameter'
    end

    echo -e '/tn\tIdentify the task to be deleted
/f\tSuppress the confirmation message
/s\tSpecify the name or IP address of a remote computer
/?\tShow help'
end

function __schtasks_end_complete_args -a previous_token
    if test "$previous_token" = /tn
        __schtasks_print_tasks
        return
    end

    if test "$previous_token" = /u
        __fish_print_windows_users
        return
    end

    if __fish_seen_argument -w s
        echo -e '/u\tRun this command with the permissions of the specified user account'
    end

    if __fish_seen_argument -w u
        echo -e '/p\tSpecify the password of the user account specified in the /u parameter'
    end

    echo -e '/tn\tIdentify the task that started the program
/s\tSpecify the name or IP address of a remote computer
/?\tShow help'
end

function __schtasks_query_complete_args -a previous_token
    if test "$previous_token" = /fo
        echo -e 'TABLE
LIST
CSV'
        return
    end

    if test "$previous_token" = /u
        __fish_print_windows_users
        return
    end

    if __fish_seen_argument -w s
        echo -e '/u\tRun this command with the permissions of the specified user account'
    end

    if __fish_seen_argument -w u
        echo -e '/p\tSpecify the password of the user account specified in the /u parameter'
    end

    echo -e '/fo\tSpecify the output format
/nh\tRemove column headings
/v\tAdd the advanced properties of the task
/s\tSpecify the name or IP address of a remote computer
/?\tShow help'
end

function __schtasks_run_complete_args -a previous_token
    if test "$previous_token" = /tn
        __schtasks_print_tasks
        return
    end

    if test "$previous_token" = /u
        __fish_print_windows_users
        return
    end

    if __fish_seen_argument -w s
        echo -e '/u\tRun this command with the permissions of the specified user account'
    end

    if __fish_seen_argument -w u
        echo -e '/p\tSpecify the password of the user account specified in the /u parameter'
    end

    echo -e '/tn\tIdentify the task to start
/s\tSpecify the name or IP address of a remote computer
/?\tShow help'
end

function __schtasks_complete_args -d 'Function to generate args'
    set --local previous_token (commandline -xc)[-1]

    if __fish_seen_argument -w change
        __schtasks_change_complete_args "$previous_token"
    else if __fish_seen_argument -w create
        __schtasks_create_complete_args "$previous_token"
    else if __fish_seen_argument -w delete
        __schtasks_delete_complete_args "$previous_token"
    else if __fish_seen_argument -w end
        __schtasks_end_complete_args "$previous_token"
    else if __fish_seen_argument -w query
        __schtasks_query_complete_args "$previous_token"
    else if __fish_seen_argument -w run
        __schtasks_run_complete_args "$previous_token"
    end
end

complete -c schtasks -f -a '(__schtasks_complete_args)'

complete -c schtasks -f -n 'not __fish_seen_argument -w change -w create -w delete -w end \
    -w query -w run' -a /change \
    -d 'Change one or more properties of a task'
complete -c schtasks -f -n 'not __fish_seen_argument -w change -w create -w delete -w end \
    -w query -w run' -a /create \
    -d 'Schedule a new task'
complete -c schtasks -f -n 'not __fish_seen_argument -w change -w create -w delete -w end \
    -w query -w run' -a /delete \
    -d 'Delete a scheduled task'
complete -c schtasks -f -n 'not __fish_seen_argument -w change -w create -w delete -w end \
    -w query -w run' -a /end \
    -d 'Stop a program started by a task'
complete -c schtasks -f -n 'not __fish_seen_argument -w change -w create -w delete -w end \
    -w query -w run' -a /query \
    -d 'Display tasks scheduled to run on the computer'
complete -c schtasks -f -n 'not __fish_seen_argument -w change -w create -w delete -w end \
    -w query -w run' -a /run \
    -d 'Start a scheduled task immediately'
