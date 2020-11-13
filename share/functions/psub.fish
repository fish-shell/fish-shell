function psub --description "Read from stdin into a file and output the filename. Remove the file when the command that called psub exits."
    set -l options -x 'f,F' -x 'F,s' h/help f/file F/fifo 's/suffix=' T-testing
    argparse -n psub --max-args=0 $options -- $argv
    or return

    if set -q _flag_help
        __fish_print_help psub
        return 0
    end

    set -l dirname
    set -l filename
    set -l funcname

    if not status --is-command-substitution
        printf (_ "%s: Not inside of command substitution") psub >&2
        return 1
    end

    set -l tmpdir /tmp
    set -q TMPDIR
    and set tmpdir $TMPDIR

    if set -q _flag_fifo
        # Write output to pipe. This needs to be done in the background so
        # that the command substitution exits without needing to wait for
        # all the commands to exit.
        set dirname (mktemp -d $tmpdir/.psub.XXXXXXXXXX)
        or return 1
        set filename $dirname/psub.fifo"$_flag_suffix"
        command mkfifo $filename
        # Note that if we were to do the obvious `cat >$filename &`, we would deadlock
        # because $filename may be opened before the fork. Use tee to ensure it is opened
        # after the fork.
        command tee $filename >/dev/null &
    else if test -z "$_flag_suffix"
        set filename (mktemp $tmpdir/.psub.XXXXXXXXXX)
        or return 1
        command cat >$filename
    else
        set dirname (mktemp -d $tmpdir/.psub.XXXXXXXXXX)
        or return 1
        set filename "$dirname/psub$_flag_suffix"
        command cat >$filename
    end

    # Write filename to stdout
    echo $filename

    # This flag isn't documented. It's strictly for our unit tests.
    if set -q _flag_testing
        return
    end

    # Find unique function name
    while true
        set funcname __fish_psub_(random)
        if not functions $funcname >/dev/null 2>/dev/null
            break
        end
    end

    # Make sure we erase file when caller exits
    function $funcname --on-job-exit caller --inherit-variable filename --inherit-variable dirname --inherit-variable funcname
        command rm $filename
        if test -n "$dirname"
            command rmdir $dirname
        end
        functions -e $funcname
    end

end
