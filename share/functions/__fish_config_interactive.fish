#
# Initializations that should only be performed when entering interactive mode.
#
# This function is called by the __fish_on_interactive function, which is defined in config.fish.
#
function __fish_config_interactive -d "Initializations that should be performed when entering interactive mode"
    # Make sure this function is only run once.
    if set -q __fish_config_interactive_done
        return
    end

    set -g __fish_config_interactive_done

    # Set the correct configuration directory
    set -l configdir ~/.config
    if set -q XDG_CONFIG_HOME
        set configdir $XDG_CONFIG_HOME
    end
    # Set the correct user data directory
    set -l userdatadir ~/.local/share
    if set -q XDG_DATA_HOME
        set userdatadir $XDG_DATA_HOME
    end

    if not set -q fish_greeting
        set -l line1 (printf (_ 'Welcome to fish, the friendly interactive shell' ))
        if not set -q __fish_init_2_3_0
            set -l line2 \n(printf (_ 'Type %shelp%s for instructions on how to use fish %s') (set_color green) (set_color normal))
        else
            set -l line2 ''
        end
        set -U fish_greeting $line1$line2
    end

    #
    # If we are starting up for the first time, set various defaults.
    #
    # bump this to 2_4_0 when rolling release if anything changes after 9/10/2016
    if not set -q __fish_init_2_39_8
        # Regular syntax highlighting colors
        # XXX - not quite the same as default colors in web config. Sync these up.
        set -q fish_color_normal
        or set -U fish_color_normal normal
        set -q fish_color_command
        or set -U fish_color_command --bold
        set -q fish_color_param
        or set -U fish_color_param cyan
        set -q fish_color_redirection
        or set -U fish_color_redirection brblue
        set -q fish_color_comment
        or set -U fish_color_comment red
        set -q fish_color_error
        or set -U fish_color_error brred
        set -q fish_color_escape
        or set -U fish_color_escape bryellow --bold
        set -q fish_color_operator
        or set -U fish_color_operator bryellow
        set -q fish_color_end
        or set -U fish_color_end brmagenta
        set -q fish_color_quote
        or set -U fish_color_quote yellow
        set -q fish_color_autosuggestion
        or set -U fish_color_autosuggestion 555 brblack
        set -q fish_color_user
        or set -U fish_color_user brgreen

        set -q fish_color_host
        or set -U fish_color_host normal
        set -q fish_color_valid_path
        or set -U fish_color_valid_path --underline

        set -q fish_color_cwd
        or set -U fish_color_cwd green
        set -q fish_color_cwd_root
        or set -U fish_color_cwd_root red

        # Background color for matching quotes and parenthesis
        set -q fish_color_match
        or set -U fish_color_match --background=brblue

        # Background color for search matches
        set -q fish_color_search_match
        or set -U fish_color_search_match bryellow --background=brblack

        # Background color for selections
        set -q fish_color_selection
        or set -U fish_color_selection white --bold --background=brblack

        # Pager colors
        set -q fish_pager_color_prefix
        or set -U fish_pager_color_prefix white --bold --underline
        set -q fish_pager_color_completion
        or set -U fish_pager_color_completion
        set -q fish_pager_color_description
        or set -U fish_pager_color_description B3A06D yellow
        set -q fish_pager_color_progress
        or set -U fish_pager_color_progress brwhite --background=cyan

        #
        # Directory history colors
        #
        set -q fish_color_history_current
        or set -U fish_color_history_current --bold


        set -U __fish_init_2_39_8
    end

    #
    # Generate man page completions if not present.
    #
    if not test -d $userdatadir/fish/generated_completions
        command -s python >/dev/null # feature needs python, don't try this on launch without it (#3588)
        # fish_update_completions is a function, so it can not be directly run in background.
        and eval (string escape "$__fish_bin_dir/fish") "-c 'fish_update_completions >/dev/null ^/dev/null' &"
    end

    #
    # Print a greeting.
    # fish_greeting can be a function (preferred) or a variable.
    #
    if status --is-interactive
        if functions -q fish_greeting
            fish_greeting
        else
            # The greeting used to be skipped when fish_greeting was empty (not just undefined)
            # Keep it that way to not print superfluous newlines on old configuration
            test -n "$fish_greeting"
            and echo $fish_greeting
        end
    end

    #
    # This event handler makes sure the prompt is repainted when
    # fish_color_cwd changes value. Like all event handlers, it can't be
    # autoloaded.
    #
    function __fish_repaint --on-variable fish_color_cwd --description "Event handler, repaints the prompt when fish_color_cwd changes"
        if status --is-interactive
            set -e __fish_prompt_cwd
            commandline -f repaint ^/dev/null
        end
    end

    function __fish_repaint_root --on-variable fish_color_cwd_root --description "Event handler, repaints the prompt when fish_color_cwd_root changes"
        if status --is-interactive
            set -e __fish_prompt_cwd
            commandline -f repaint ^/dev/null
        end
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

    # Make sure some key bindings are set
    if not set -q fish_key_bindings
        set -U fish_key_bindings fish_default_key_bindings
    end

    # Reload key bindings when binding variable change
    function __fish_reload_key_bindings -d "Reload key bindings when binding variable change" --on-variable fish_key_bindings
        # do nothing if the key bindings didn't actually change
        # This could be because the variable was set to the existing value
        # or because it was a local variable
        # If fish_key_bindings is empty on the first run, we still need to set the defaults
        if test "$fish_key_bindings" = "$__fish_active_key_bindings" -a -n "$fish_key_bindings"
            return
        end
        # Check if fish_key_bindings is a valid function
        # If not, either keep the previous bindings (if any) or revert to default
        # Also print an error so the user knows
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
        if test "$fish_key_bindings" = fish_default_key_bindings
            # Redirect stderr per #1155
            fish_default_key_bindings ^/dev/null
        else
            eval $fish_key_bindings ^/dev/null
        end
        # Load user key bindings if they are defined
        if functions --query fish_user_key_bindings >/dev/null
            fish_user_key_bindings ^/dev/null
        end
    end

    # Load key bindings
    __fish_reload_key_bindings

    function __fish_winch_handler --on-signal WINCH -d "Repaint screen when window changes size"
        commandline -f repaint
    end

    # Notify terminals when $PWD changes (issue #906)
    # VTE and Terminal.app support this in practice.
    if test "0$VTE_VERSION" -ge 3405 -o "$TERM_PROGRAM" = "Apple_Terminal"
        function __update_cwd_osc --on-variable PWD --description 'Notify capable terminals when $PWD changes'
            status --is-command-substitution
            or test -n "$INSIDE_EMACS"
            and return
            printf \e\]7\;file://\%s\%s\a (hostname) (echo -n $PWD | __fish_urlencode)
        end
        if test "$TERM_PROGRAM" = "Apple_Terminal"
            # Suppress duplicative title display on Terminal.app
            if not functions -q fish_title
                echo -n \e\]0\;\a # clear existing title
                function fish_title -d 'no-op terminal title'
                end
            end
        end
        __update_cwd_osc # Run once because we might have already inherited a PWD from an old tab
    end

    ### Command-not-found handlers
    # This can be overridden by defining a new __fish_command_not_found_handler function
    if not type -q __fish_command_not_found_handler
        # Read the OS/Distro from /etc/os-release.
        # This has a "ID=" line that defines the exact distribution,
        # and an "ID_LIKE=" line that defines what it is derived from or otherwise like.
        # For our purposes, we use both.
        set -l os
        if test -r /etc/os-release
            set os (string match -r '^ID(?:_LIKE)?\s*=.*' < /etc/os-release | \
            string replace -r '^ID(?:_LIKE)?\s*=(.*)' '$1' | string trim -c '\'"')
        end

        # First check if we are on OpenSUSE since SUSE's handler has no options
        # but the same name and path as Ubuntu's.
        if contains -- suse $os
            and type -q -p command-not-found
            function __fish_command_not_found_handler --on-event fish_command_not_found
                /usr/bin/command-not-found $argv[1]
            end
            # Check for Fedora's handler
        else if test -f /usr/libexec/pk-command-not-found
            function __fish_command_not_found_handler --on-event fish_command_not_found
                /usr/libexec/pk-command-not-found $argv[1]
            end
            # Check in /usr/lib, this is where modern Ubuntus place this command
        else if test -f /usr/lib/command-not-found
            function __fish_command_not_found_handler --on-event fish_command_not_found
                /usr/lib/command-not-found -- $argv[1]
            end
            # Check for NixOS handler
        else if test -f /run/current-system/sw/bin/command-not-found
            function __fish_command_not_found_handler --on-event fish_command_not_found
                /run/current-system/sw/bin/command-not-found $argv
            end
            # Ubuntu Feisty places this command in the regular path instead
        else if type -q -p command-not-found
            function __fish_command_not_found_handler --on-event fish_command_not_found
                command-not-found -- $argv[1]
            end
            # pkgfile is an optional, but official, package on Arch Linux
            # it ships with example handlers for bash and zsh, so we'll follow that format
        else if type -p -q pkgfile
            function __fish_command_not_found_handler --on-event fish_command_not_found
                set -l __packages (pkgfile --binaries --verbose -- $argv[1] ^/dev/null)
                if test $status -eq 0
                    printf "%s may be found in the following packages:\n" "$argv[1]"
                    printf "  %s\n" $__packages
                else
                    __fish_default_command_not_found_handler $argv[1]
                end
            end
            # Use standard fish command not found handler otherwise
        else
            function __fish_command_not_found_handler --on-event fish_command_not_found
                __fish_default_command_not_found_handler $argv[1]
            end
        end
    end
end
