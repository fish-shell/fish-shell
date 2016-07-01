function suspend -d "Suspend the current shell."
    if begin contains -- $argv --force
            or not status --is-interactive and not status --is-login
        end
        printf "Suspending %d\n%s fg%s to resume" %self (set_color --bold) (set_color normal)
        if contains -- $argv --force
            printf " (or%s kill -CONT %d%s from another terminal)\n" (set_color --bold) %self (set_color normal)
        else
            echo
        end

        kill -STOP %self
    else
        echo 2>&1 "Refusing to suspend login shell."
        echo 2>&1 "Use --force to override. This might hang your terminal."
    end
end
