function suspend --description 'Suspend the current shell.'
    set -l options h/help f/force
    argparse -n suspend --max-args=1 $options -- $argv
    or return

    if set -q _flag_help
        __fish_print_help suspend
        return 0
    end

    if not set -q _flag_force
        and status is-login
        and status is-interactive

        echo 2>&1 'Refusing to suspend interactive login shell.'
        echo 2>&1 'Use --force to override. This might hang your terminal.'
        return 1
    end

    if status is-interactive
        echo -ns 'Suspending ' $fish_pid ': run'
        echo -n (set_color --bold) 'kill -CONT' $fish_pid (set_color normal)
        echo 'from another terminal to resume'
    end

    # XXX always causes a zombie until one fg's when we do this:
    kill -STOP $fish_pid
end
