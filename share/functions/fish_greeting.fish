function fish_greeting
    if not set -q fish_greeting
        set -g fish_greeting (random choice 🐟 🐠 🐡)
    end

    if set -q fish_private_mode
        set -l line (_ "fish is running in private mode, history will not be persisted.")
        set -g fish_greeting $fish_greeting.\n$line
    end

    # The greeting used to be skipped when fish_greeting was empty (not just undefined)
    # Keep it that way to not print superfluous newlines on old configuration
    test -n "$fish_greeting"
    and echo $fish_greeting
end
