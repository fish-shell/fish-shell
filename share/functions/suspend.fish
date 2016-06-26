function suspend -d "Suspend the current shell."
    if begin contains -- $argv --force
             or not status --is-interactive and not status --is-login
       end
       echo Suspending %self
       kill -STOP %self
    else
       echo 2>&1 Cannot suspend login shell. Use --force to override.
    end
end
