#
# Initializations that should only be performed when entering interactive mode.
#
# This function is called by the __fish_on_interactive function, which is defined in config.fish.
#
function __fish_config_interactive -d "Initializations that should be performed when entering interactive mode"
    if test $__fish_initialized -lt 3000
        # Perform transitions relevant to going from fish 2.x to 3.x.

        # Migrate old universal abbreviations to the new scheme.
        __fish_abbr_old | source
    end

    # Make sure this function is only run once.
    if set -q __fish_config_interactive_done
        return
    end

    set -g __fish_config_interactive_done
    set -g __fish_active_key_bindings

    if not set -q fish_greeting
        set -l line1 (_ 'Welcome to fish, the friendly interactive shell')
        set -l line2 ''
        if test $__fish_initialized -lt 2300
            set line2 \n(_ 'Type `help` for instructions on how to use fish')
        end
        set -U fish_greeting "$line1$line2"
    end

    if set -q fish_private_mode; and string length -q -- $fish_greeting
        set -l line (_ "fish is running in private mode, history will not be persisted.")
        set -g fish_greeting $fish_greeting.\n$line
    end

    # usage: __init_uvar VARIABLE VALUES...
    function __init_uvar -d "Sets a universal variable if it's not already set"
        if not set --query $argv[1]
            set --universal $argv
        end
    end

    #
    # If we are starting up for the first time, set various defaults.
    if test $__fish_initialized -lt 3100

        # Regular syntax highlighting colors
        __init_uvar fish_color_normal normal
        __init_uvar fish_color_command 005fd7
        __init_uvar fish_color_param 00afff
        __init_uvar fish_color_redirection 00afff
        __init_uvar fish_color_comment 990000
        __init_uvar fish_color_error ff0000
        __init_uvar fish_color_escape 00a6b2
        __init_uvar fish_color_operator 00a6b2
        __init_uvar fish_color_end 009900
        __init_uvar fish_color_quote 999900
        __init_uvar fish_color_autosuggestion 555 brblack
        __init_uvar fish_color_user brgreen
        __init_uvar fish_color_host normal
        __init_uvar fish_color_host_remote yellow
        __init_uvar fish_color_valid_path --underline
        __init_uvar fish_color_status red

        __init_uvar fish_color_cwd green
        __init_uvar fish_color_cwd_root red

        # Background color for matching quotes and parenthesis
        __init_uvar fish_color_match --background=brblue

        # Background color for search matches
        __init_uvar fish_color_search_match bryellow --background=brblack

        # Background color for selections
        __init_uvar fish_color_selection white --bold --background=brblack

        # XXX fish_color_cancel was added in 2.6, but this was added to post-2.3 initialization
        # when 2.4 and 2.5 were already released
        __init_uvar fish_color_cancel -r

        # Pager colors
        __init_uvar fish_pager_color_prefix white --bold --underline
        __init_uvar fish_pager_color_completion
        __init_uvar fish_pager_color_description B3A06D yellow
        __init_uvar fish_pager_color_progress brwhite --background=cyan

        #
        # Directory history colors
        #
        __init_uvar fish_color_history_current --bold
    end

    #
    # Generate man page completions if not present.
    #
    # Don't do this if we're being invoked as part of running unit tests.
    if not set -q FISH_UNIT_TESTS_RUNNING
        if not test -d $__fish_user_data_dir/generated_completions
            # Generating completions from man pages needs python (see issue #3588).

            # We cannot simply do `fish_update_completions &` because it is a function.
            # We cannot do `eval` since it is a function.
            # We don't want to call `fish -c` since that is unnecessary and sources config.fish again.
            # Hence we'll call python directly.
            # c_m_p.py should work with any python version.
            set -l update_args -B $__fish_data_dir/tools/create_manpage_completions.py --manpath --cleanup-in '~/.config/fish/completions' --cleanup-in '~/.config/fish/generated_completions'
            for py in python{3,2,}
                if command -sq $py
                    set -l c $py $update_args
                    # Run python directly in the background and swallow all output
                    $c (: fish_update_completions: generating completions from man pages) >/dev/null 2>&1 &
                    # Then disown the job so that it continues to run in case of an early exit (#6269)
                    disown 2>&1 >/dev/null
                    break
                end
            end
        end
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
    # fish_color_cwd{,_root} changes value. Like all event handlers, it can't be
    # autoloaded.
    #
    set -l varargs --on-variable fish_key_bindings
    for var in user host cwd{,_root} status
        set -a varargs --on-variable fish_color_$var
    end
    function __fish_repaint $varargs -d "Event handler, repaints the prompt when fish_color_cwd* changes"
        if status --is-interactive
            set -e __fish_prompt_cwd
            commandline -f repaint 2>/dev/null
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

    #
    # We want to show our completions for the [ (test) builtin, but
    # we don't want to create a [.fish. test.fish will not be loaded until
    # the user tries [ interactively.
    #
    complete -c [ --wraps test
    complete -c ! --wraps not

    #
    # Only a few builtins take filenames; initialize the rest with no file completions
    #
    complete -c(builtin -n | string match -rv '(source|cd|exec|realpath|set|\\[|test|for)') --no-files

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
        if test "$fish_key_bindings" = fish_default_key_bindings
            # Redirect stderr per #1155
            fish_default_key_bindings 2>/dev/null
        else
            $fish_key_bindings 2>/dev/null
        end
        # Load user key bindings if they are defined
        if functions --query fish_user_key_bindings >/dev/null
            fish_user_key_bindings 2>/dev/null
        end
    end

    # Load key bindings
    __fish_reload_key_bindings

    if not set -q FISH_UNIT_TESTS_RUNNING
        # Enable bracketed paste before every prompt (see __fish_shared_bindings for the bindings).
        # Disable it for unit tests so we don't have to add the sequences to bind.expect
        function __fish_enable_bracketed_paste --on-event fish_prompt
            printf "\e[?2004h"
        end

        # Disable BP before every command because that might not support it.
        function __fish_disable_bracketed_paste --on-event fish_preexec --on-event fish_exit
            printf "\e[?2004l"
        end

        # Tell the terminal we support BP. Since we are in __f_c_i, the first fish_prompt
        # has already fired.
        __fish_enable_bracketed_paste
    end

    # Similarly, enable TMUX's focus reporting when in tmux.
    # This will be handled by
    # - The keybindings (reading the sequence and triggering an event)
    # - Any listeners (like the vi-cursor)
    if set -q TMUX
        and not set -q FISH_UNIT_TESTS_RUNNING
        function __fish_enable_focus --on-event fish_postexec
            echo -n \e\[\?1004h
        end
        function __fish_disable_focus --on-event fish_preexec
            echo -n \e\[\?1004l
        end
        # Note: Don't call this initially because, even though we're in a fish_prompt event,
        # tmux reacts sooo quickly that we'll still get a sequence before we're prepared for it.
        # So this means that we won't get focus events until you've run at least one command, but that's preferable
        # to always seeing `^[[I` when starting fish.
        # __fish_enable_focus
    end

    function __fish_winch_handler --on-signal WINCH -d "Repaint screen when window changes size"
        commandline -f repaint >/dev/null 2>/dev/null
    end

    # Notify terminals when $PWD changes (issue #906).
    # VTE based terminals, Terminal.app, and iTerm.app (TODO) support this.
    if test 0"$VTE_VERSION" -ge 3405 -o "$TERM_PROGRAM" = "Apple_Terminal" -a (string match -r '\d+' 0"$TERM_PROGRAM_VERSION") -ge 309
        function __update_cwd_osc --on-variable PWD --description 'Notify capable terminals when $PWD changes'
            if status --is-command-substitution || set -q INSIDE_EMACS
                return
            end
            printf \e\]7\;file://%s%s\a $hostname (string escape --style=url $PWD)
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
            string replace -r '^ID(?:_LIKE)?\s*=(.*)' '$1' | string trim -c '\'"' | string split " ")
        end

        # First check if we are on OpenSUSE since SUSE's handler has no options
        # but the same name and path as Ubuntu's.
        if contains -- suse $os || contains -- sles $os && type -q command-not-found
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
        else if type -q command-not-found
            function __fish_command_not_found_handler --on-event fish_command_not_found
                command-not-found -- $argv[1]
            end
            # pkgfile is an optional, but official, package on Arch Linux
            # it ships with example handlers for bash and zsh, so we'll follow that format
        else if type -p -q pkgfile
            function __fish_command_not_found_handler --on-event fish_command_not_found
                set -l __packages (pkgfile --binaries --verbose -- $argv[1] 2>/dev/null)
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

    # Bump this whenever some code below needs to run once when upgrading to a new version.
    # The universal variable __fish_initialized is initialized in share/config.fish.
    set __fish_initialized 3100
end
