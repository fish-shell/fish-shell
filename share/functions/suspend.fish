function suspend -d "Suspend the current shell."
    if begin contains -- $argv --force
            or not status --is-interactive and not status --is-login
        end
        printf "Suspending %d: %sfg%s to resume" %self (set_color --bold) (set_color normal)
        if contains -- $argv --force
            printf " (or%s kill -CONT %d%s from another terminal)" (set_color --bold) %self (set_color normal)
        end
        # XXX not sure if this echo should be necessary, but without it, it seems
        # everything printf'd above will not get pushed back to stdout before the suspend
        echo ""
        # XXX always causes a zombie until one fg's when we do this:
        kill -STOP %self
    else
        echo 2>&1 "Refusing to suspend login shell."
        echo 2>&1 "Use --force to override. This might hang your terminal."
    end
end
