function __fish_complete_proc --description 'Complete by list of running processes'
    set -l ps_cmd
    set -l sed_cmds
    if test (uname) = Linux    
        # Displays processes
        set ps_cmd 'ps eu'
        # Prints the program name
        set awk_cmds $awk_cmds '{print $11}'
        # Removes everything up to the last / and deletes first line
        set sed_cmds $sed_cmds 's/.*\///;1d'
        eval $ps_cmd | awk $awk_cmds | sed $sed_cmds | sort -u
    else
        # OS X, BSD. Preserve leading spaces.
        set ps_cmd 'ps axc -o comm'
        # Delete parenthesized (zombie) processes
        set sed_cmds $sed_cmds '/(.*)/d'    
        eval $ps_cmd | sed $sed_cmds | sort -u
    end
end
