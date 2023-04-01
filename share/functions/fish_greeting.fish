function ghoti_greeting
    if not set -q ghoti_greeting
        set -l line1 (_ 'Welcome to ghoti, the friendly interactive shell')
        set -l line2 \n(printf (_ 'Type %shelp%s for instructions on how to use ghoti') (set_color green) (set_color normal))
        set -g ghoti_greeting "$line1$line2"
    end

    if set -q ghoti_private_mode
        set -l line (_ "ghoti is running in private mode, history will not be persisted.")
        if set -q ghoti_greeting[1]
            set -g ghoti_greeting $ghoti_greeting\n$line
        else
            set -g ghoti_greeting $line
        end
    end

    # The greeting used to be skipped when ghoti_greeting was empty (not just undefined)
    # Keep it that way to not print superfluous newlines on old configuration
    test -n "$ghoti_greeting"
    and echo $ghoti_greeting
end
