function eval -S -d "Evaluate parameters as a command"
    # keep a copy of the previous $status and use restore_status
    # to preserve the status in case the block that is evaluated
    # does not modify the status itself.
    set -l status_copy $status
    function __fish_restore_status
        return $argv[1]
    end

    if not set -q argv[2]
        # like most builtins, we only check for -h/--help
        # if we only have a single argument
        switch "$argv[1]"
            case -h --help
                __fish_print_help eval
                return 0
        end
    end

    # If we are in an interactive shell, eval should enable full
    # job control since it should behave like the real code was
    # executed.  If we don't do this, commands that expect to be
    # used interactively, like less, wont work using eval.

    set -l mode
    if status --is-interactive-job-control
        set mode interactive
    else
        if status --is-full-job-control
            set mode full
        else
            set mode none
        end
    end
    if status --is-interactive
        status --job-control full
    end
    __fish_restore_status $status_copy

    # To eval 'foo', we construct a block "begin ; foo; end <&3 3<&-"
    # Note the redirections are also within the quotes.
    #
    # We then pipe this to 'source 3<&0’.
    #
    # You might expect that the dup2(3, stdin) should overwrite stdin,
    # and therefore prevent 'source' from reading the piped-in block. This doesn't happen
    # because when you pipe to a builtin, we don't overwrite stdin with the read end
    # of the block; instead we set a separate fd in a variable 'builtin_stdin', which is
    # what it reads from. So builtins are magic in that, in pipes, their stdin
    # is not fd 0.
    #
    # ‘source’ does not apply the redirections to itself. Instead it saves them and passes
    # them as block-level redirections to parser.eval(). Ultimately the eval’d code sees
    # the following redirections (in the following order):
    #    dup2 0 -> 3
    #    dup2 pipe -> 0
    #    dup2 3 -> 0
    # where the pipe is the pipe we get from piping ‘echo’ to ‘source’. Thus the redirection
    # effectively makes stdin fd0, instead of the thing that was piped to ‘source’
    echo "begin; $argv "\n" ;end <&3 3<&-" | source 3<&0
    set -l res $status

    status --job-control $mode
    return $res
end
