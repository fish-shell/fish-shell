# localization: skip(private)
#
# Initializations that should only be performed when entering interactive mode.
#
# This function is called by the __fish_on_interactive function, which is defined in config.fish.
#
function __fish_config_interactive -d "Initializations that should be performed when entering interactive mode"
    # For one-off upgrades of the fish version
    if not set -q __fish_initialized
        set -U __fish_initialized 0
    end

    set -g __fish_active_key_bindings

    # usage: __init_uvar VARIABLE VALUES...
    function __init_uvar -d "Sets a universal variable if it's not already set"
        if not set --query $argv[1]
            set --universal $argv
        end
    end

    # If we are starting up for the first time, set various defaults.
    if test $__fish_initialized -lt 3400
        # Create empty configuration directores if they do not already exist
        test -e $__fish_config_dir/completions/ -a -e $__fish_config_dir/conf.d/ -a -e $__fish_config_dir/functions/ ||
            mkdir -p $__fish_config_dir/{completions, conf.d, functions}

        # Create config.fish with some boilerplate if it does not exist
        test -e $__fish_config_dir/config.fish || echo "\
if status is-interactive
    # Commands to run in interactive sessions can go here
end" >$__fish_config_dir/config.fish

        echo yes | fish_config theme save "fish default"
        set -Ue fish_color_keyword fish_color_option
    end
    if test $__fish_initialized -lt 3800 && test "$fish_color_search_match[1]" = bryellow
        set --universal fish_color_search_match[1] white
    end

    #
    # Generate man page completions if not present.
    #
    # Don't do this if we're being invoked as part of running unit tests.
    if not set -q FISH_UNIT_TESTS_RUNNING
        if not test -d $__fish_cache_dir/generated_completions
            # We cannot simply do `fish_update_completions &` because it is a function.
            # We don't want to call `fish -c` since that is unnecessary and sources config.fish again.
            mkdir -p $__fish_cache_dir/generated_completions
            fish_update_completions_detach=true fish_update_completions 2>/dev/null
        end
    end

    #
    # Print a greeting.
    # The default just prints a variable of the same name.
    #
    # NOTE: This status check is necessary to not print the greeting when `read`ing in scripts. See #7080.
    if status --is-interactive
        and functions -q fish_greeting
        fish_greeting
    end

    #
    # Completions for SysV startup scripts. These aren't bound to any
    # specific command, so they can't be autoloaded.
    #
    if test -d /etc/init.d
        complete -x -p "/etc/init.d/*" -a start --description 'Start service'
        complete -x -p "/etc/init.d/*" -a stop --description 'Stop service'
        complete -x -p "/etc/init.d/*" -a status --description 'Print service status'
        complete -x -p "/etc/init.d/*" -a restart --description 'Stop and then start service'
        complete -x -p "/etc/init.d/*" -a reload --description 'Reload service configuration'
    end

    #
    # Only a few builtins take filenames; initialize the rest with no file completions
    #
    complete -c(builtin -n | string match -rv '(\.|:|source|cd|contains|count|echo|exec|fish_indent|printf|random|realpath|set|\\[|test|for)') --no-files

    # Reload key bindings when binding variable change
    function __fish_reload_key_bindings -d "Reload key bindings when binding variable change" --on-variable fish_key_bindings
        # Make sure some key bindings are set
        __init_uvar fish_key_bindings fish_default_key_bindings

        # Do nothing if the key bindings didn't actually change.
        # This could be because the variable was set to the existing value
        # or because it was a local variable.
        # If fish_key_bindings is empty on the first run, we still need to set the defaults.
        if test "$fish_key_bindings" = "$__fish_active_key_bindings" -a -n "$fish_key_bindings"
            return
        end
        # Check if fish_key_bindings is a valid function.
        # If not, either keep the previous bindings (if any) or revert to default.
        # Also print an error so the user knows.
        if not functions -q "$fish_key_bindings"
            echo "There is no fish_key_bindings function called: '$fish_key_bindings'" >&2
            # We need to see if this is a defined function, otherwise we'd be in an endless loop.
            if functions -q $__fish_active_key_bindings
                echo "Keeping $__fish_active_key_bindings" >&2
                # Set the variable to the old value so this error doesn't happen again.
                set fish_key_bindings $__fish_active_key_bindings
                return 1
            else if functions -q fish_default_key_bindings
                echo "Reverting to default bindings" >&2
                set fish_key_bindings fish_default_key_bindings
                # Return because we are called again
                return 0
            else
                # If we can't even find the default bindings, something is broken.
                # Without it, we would eventually run into the stack size limit, but that'd print hundreds of duplicate lines
                # so we should give up earlier.
                echo "Cannot find fish_default_key_bindings, falling back to very simple bindings." >&2
                echo "Most likely something is wrong with your installation." >&2
                return 0
            end
        end
        set -g __fish_active_key_bindings "$fish_key_bindings"
        set -g fish_bind_mode default
        $fish_key_bindings
        # Load user key bindings if they are defined
        if functions --query fish_user_key_bindings >/dev/null
            fish_user_key_bindings
        end
    end

    # Load key bindings
    __fish_reload_key_bindings

    # Detect whether the terminal reflows on its own
    # If it does we shouldn't do it.
    # Allow $fish_handle_reflow to override it.
    if not set -q fish_handle_reflow
        # VTE reflows the text itself, so us doing it inevitably races against it.
        # Guidance from the VTE developers is to let them repaint.
        # Konsole reflows since version 21.04. Konsole added XTVERSION
        # in v22.03.80~7.
        # TODO(term-workaround)
        if string match -rq -- '^(?:VTE\b|Konsole |WezTerm )' (status terminal)
            or begin
                set -q KONSOLE_VERSION
                and test "$KONSOLE_VERSION" -ge 210400 2>/dev/null
            end
            or string match -q -- 'alacritty*' $TERM
            set -g fish_handle_reflow 0
        else
            set -g fish_handle_reflow 1
        end
    end

    function __fish_winch_handler --on-signal WINCH -d "Repaint screen when window changes size"
        if test "$fish_handle_reflow" = 1 2>/dev/null
            commandline -f repaint >/dev/null 2>/dev/null
        end
    end

    # Notify terminals when $PWD changes via OSC 7 (issue #906).
    if not functions --query __fish_update_cwd_osc
        function __fish_update_cwd_osc --on-variable PWD --description 'Notify terminals when $PWD changes'
            set -l host $hostname
            # if set -l konsole_version (string match -r -- '^Konsole (\d+)\..*' (status terminal))[2]
            #     # To-do: use a Konsole version where KF6_DEP_VERSION is >= 6.12
            #     and $konsole_version -lt ???
            # end
            # TODO(term-workaround)
            if set -q KONSOLE_VERSION
                set host ''
            end
            if [ -n "$MSYSTEM" ] && string match -rq -- '^(Konsole|WezTerm) ' (status terminal)
                return
            end
            if [ "$TERM" = dumb ]
                return
            end
            printf \e\]7\;file://%s%s\a $host (string escape --style=url -- $PWD)
        end
    end
    __fish_update_cwd_osc # Run once because we might have already inherited a PWD from an old tab

    # Bump this whenever some code below needs to run once when upgrading to a new version.
    # The universal variable __fish_initialized is initialized in share/config.fish.
    set __fish_initialized 3800

    functions -e __fish_config_interactive
end
