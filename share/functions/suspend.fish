# If the user hasn't set it, make sure our test level has a value
if not set -q suspend_minimum_SHLVL
    set -g suspend_minimum_SHLVL 3
end


function suspend -d "Suspend the current shell."
    if begin contains -- $argv --force
             or not status --is-interactive
             or begin test $SHLVL -ge $suspend_minimum_SHLVL
                      and not status --is-login
                end
       end
       kill -STOP %self
    else
       echo 2>&1 Cannot suspend login shell or SHLVL to low, use --force to force.
    end
end
