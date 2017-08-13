function __fish_complete_proc --description 'Complete by list of running processes'
    # Our function runs ps, followed by a massive list of commands passed to sed
    set -l ps_cmd
    set -l sed_cmds
    if test (uname) = Linux
        # comm and ucomm return a truncated name, so parse it from the command line field,
        # which means we have to trim off the arguments.
        # Unfortunately, it doesn't seem to escape spaces - so we can't distinguish
        # between the command name, and the first argument. Still, processes with spaces
        # in the name seem more common on OS X than on Linux, so prefer to parse out the
        # command line rather than using the stat data.
        # If the command line is unavailable, you get the stat data in brackets - so
        # parse out brackets too.
        set ps_opt -A -o command

        # Erase everything after the first space
        set sed_cmds $sed_cmds 's/ .*//'

        # Erases weird stuff Linux gives like kworker/0:0
        set sed_cmds $sed_cmds 's|/[0-9]:[0-9]]$||g'

        # Retain the last path component only
        set sed_cmds $sed_cmds 's|.*/||g'

        # Strip off square brackets. Cute, huh?
        set sed_cmds $sed_cmds 's/[][]//g'

        # Erase things that are just numbers
        set sed_cmds $sed_cmds 's/^[0-9]*$//'
    else
        # OS X, BSD. Preserve leading spaces.
        set ps_opt axc -o comm

        # Delete parenthesized (zombie) processes
        set sed_cmds $sed_cmds '/(.*)/d'
    end

    # Append sed command to delete first line (the header)
    set sed_cmds $sed_cmds '1d'

    # Append sed commands to delete leading dashes and trailing spaces
    # In principle, commands may have trailing spaces, but ps emits space padding on OS X
    set sed_cmds $sed_cmds 's/^-//' 's/ *$//'

    # Run ps, pipe it through our massive set of sed commands, then sort and unique
    ps $ps_opt | sed '-e '$sed_cmds | sort -u
end
