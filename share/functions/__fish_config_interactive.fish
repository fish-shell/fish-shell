#
# Initializations that should only be performed when entering interactive mode.
#
# This function is called by the __ghoti_on_interactive function, which is defined in config.ghoti.
#
function __ghoti_config_interactive -d "Initializations that should be performed when entering interactive mode"
    # For one-off upgrades of the ghoti version
    if not set -q __ghoti_initialized
        set -U __ghoti_initialized 0
    end

    set -g __ghoti_active_key_bindings

    # usage: __init_uvar VARIABLE VALUES...
    function __init_uvar -d "Sets a universal variable if it's not already set"
        if not set --query $argv[1]
            set --universal $argv
        end
    end

    # If we are starting up for the first time, set various defaults.
    if test $__ghoti_initialized -lt 3400
        # Create empty configuration directores if they do not already exist
        test -e $__ghoti_config_dir/completions/ -a -e $__ghoti_config_dir/conf.d/ -a -e $__ghoti_config_dir/functions/ ||
            mkdir -p $__ghoti_config_dir/{completions, conf.d, functions}

        # Create config.ghoti with some boilerplate if it does not exist
        test -e $__ghoti_config_dir/config.ghoti || echo "\
if status is-interactive
    # Commands to run in interactive sessions can go here
end" >$__ghoti_config_dir/config.ghoti

        # Regular syntax highlighting colors
        __init_uvar ghoti_color_normal normal
        __init_uvar ghoti_color_command blue
        __init_uvar ghoti_color_param cyan
        __init_uvar ghoti_color_redirection cyan --bold
        __init_uvar ghoti_color_comment red
        __init_uvar ghoti_color_error brred
        __init_uvar ghoti_color_escape brcyan
        __init_uvar ghoti_color_operator brcyan
        __init_uvar ghoti_color_end green
        __init_uvar ghoti_color_quote yellow
        __init_uvar ghoti_color_autosuggestion 555 brblack
        __init_uvar ghoti_color_user brgreen
        __init_uvar ghoti_color_host normal
        __init_uvar ghoti_color_host_remote yellow
        __init_uvar ghoti_color_valid_path --underline
        __init_uvar ghoti_color_status red

        __init_uvar ghoti_color_cwd green
        __init_uvar ghoti_color_cwd_root red

        # Background color for search matches
        __init_uvar ghoti_color_search_match bryellow --background=brblack

        # Background color for selections
        __init_uvar ghoti_color_selection white --bold --background=brblack

        # XXX ghoti_color_cancel was added in 2.6, but this was added to post-2.3 initialization
        # when 2.4 and 2.5 were already released
        __init_uvar ghoti_color_cancel -r

        # Pager colors
        __init_uvar ghoti_pager_color_prefix normal --bold --underline
        __init_uvar ghoti_pager_color_completion normal
        __init_uvar ghoti_pager_color_description B3A06D yellow -i
        __init_uvar ghoti_pager_color_progress brwhite --background=cyan
        __init_uvar ghoti_pager_color_selected_background -r

        #
        # Directory history colors
        #
        __init_uvar ghoti_color_history_current --bold
    end

    #
    # Generate man page completions if not present.
    #
    # Don't do this if we're being invoked as part of running unit tests.
    if not set -q FISH_UNIT_TESTS_RUNNING
        # Check if our manpage completion script exists because some distros split it out.
        # (#7183)
        set -l script $__ghoti_data_dir/tools/create_manpage_completions.py
        if not test -d $__ghoti_user_data_dir/generated_completions; and test -e "$script"
            # Generating completions from man pages needs python (see issue #3588).

            # We cannot simply do `ghoti_update_completions &` because it is a function.
            # We cannot do `eval` since it is a function.
            # We don't want to call `ghoti -c` since that is unnecessary and sources config.ghoti again.
            # Hence we'll call python directly.
            # c_m_p.py should work with any python version.
            set -l update_args -B $__ghoti_data_dir/tools/create_manpage_completions.py --manpath --cleanup-in '~/.config/ghoti/completions' --cleanup-in '~/.config/ghoti/generated_completions'
            if set -l python (__ghoti_anypython)
                # Run python directly in the background and swallow all output
                $python $update_args >/dev/null 2>&1 &
                # Then disown the job so that it continues to run in case of an early exit (#6269)
                disown >/dev/null 2>&1
            end
        end
    end

    #
    # Print a greeting.
    # The default just prints a variable of the same name.
    #
    # NOTE: This status check is necessary to not print the greeting when `read`ing in scripts. See #7080.
    if status --is-interactive
        and functions -q ghoti_greeting
        ghoti_greeting
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
    # we don't want to create a [.ghoti. test.ghoti will not be loaded until
    # the user tries [ interactively.
    #
    complete -c [ --wraps test
    complete -c ! --wraps not

    #
    # Only a few builtins take filenames; initialize the rest with no file completions
    #
    complete -c(builtin -n | string match -rv '(\.|:|source|cd|contains|count|echo|exec|printf|random|realpath|set|\\[|test|for)') --no-files

    # Reload key bindings when binding variable change
    function __ghoti_reload_key_bindings -d "Reload key bindings when binding variable change" --on-variable ghoti_key_bindings
        # Make sure some key bindings are set
        __init_uvar ghoti_key_bindings ghoti_default_key_bindings

        # Do nothing if the key bindings didn't actually change.
        # This could be because the variable was set to the existing value
        # or because it was a local variable.
        # If ghoti_key_bindings is empty on the first run, we still need to set the defaults.
        if test "$ghoti_key_bindings" = "$__ghoti_active_key_bindings" -a -n "$ghoti_key_bindings"
            return
        end
        # Check if ghoti_key_bindings is a valid function.
        # If not, either keep the previous bindings (if any) or revert to default.
        # Also print an error so the user knows.
        if not functions -q "$ghoti_key_bindings"
            echo "There is no ghoti_key_bindings function called: '$ghoti_key_bindings'" >&2
            # We need to see if this is a defined function, otherwise we'd be in an endless loop.
            if functions -q $__ghoti_active_key_bindings
                echo "Keeping $__ghoti_active_key_bindings" >&2
                # Set the variable to the old value so this error doesn't happen again.
                set ghoti_key_bindings $__ghoti_active_key_bindings
                return 1
            else if functions -q ghoti_default_key_bindings
                echo "Reverting to default bindings" >&2
                set ghoti_key_bindings ghoti_default_key_bindings
                # Return because we are called again
                return 0
            else
                # If we can't even find the default bindings, something is broken.
                # Without it, we would eventually run into the stack size limit, but that'd print hundreds of duplicate lines
                # so we should give up earlier.
                echo "Cannot find ghoti_default_key_bindings, falling back to very simple bindings." >&2
                echo "Most likely something is wrong with your installation." >&2
                return 0
            end
        end
        set -g __ghoti_active_key_bindings "$ghoti_key_bindings"
        set -g ghoti_bind_mode default
        if test "$ghoti_key_bindings" = ghoti_default_key_bindings
            # Redirect stderr per #1155
            ghoti_default_key_bindings 2>/dev/null
        else
            $ghoti_key_bindings 2>/dev/null
        end
        # Load user key bindings if they are defined
        if functions --query ghoti_user_key_bindings >/dev/null
            ghoti_user_key_bindings 2>/dev/null
        end
    end

    # Load key bindings
    __ghoti_reload_key_bindings

    # Enable bracketed paste exception when running unit tests so we don't have to add
    # the sequences to bind.expect
    if not set -q FISH_UNIT_TESTS_RUNNING
        # Enable bracketed paste before every prompt (see __ghoti_shared_bindings for the bindings).
        # We used to do this for read, but that would break non-interactive use and
        # compound commandlines like `read; cat`, because
        # it won't disable it after the read.
        function __ghoti_enable_bracketed_paste --on-event ghoti_prompt
            printf "\e[?2004h"
        end

        # Disable BP before every command because that might not support it.
        function __ghoti_disable_bracketed_paste --on-event ghoti_preexec --on-event ghoti_exit
            printf "\e[?2004l"
        end

        # Tell the terminal we support BP. Since we are in __f_c_i, the first ghoti_prompt
        # has already fired.
        # But only if we're interactive, in case we are in `read`
        status is-interactive
        and __ghoti_enable_bracketed_paste
    end

    # Similarly, enable TMUX's focus reporting when in tmux.
    # This will be handled by
    # - The keybindings (reading the sequence and triggering an event)
    # - Any listeners (like the vi-cursor)
    if set -q TMUX
        and not set -q FISH_UNIT_TESTS_RUNNING
        function __ghoti_enable_focus --on-event ghoti_postexec
            echo -n \e\[\?1004h
        end
        function __ghoti_disable_focus --on-event ghoti_preexec
            echo -n \e\[\?1004l
        end
        # Note: Don't call this initially because, even though we're in a ghoti_prompt event,
        # tmux reacts sooo quickly that we'll still get a sequence before we're prepared for it.
        # So this means that we won't get focus events until you've run at least one command, but that's preferable
        # to always seeing `^[[I` when starting ghoti.
        # __ghoti_enable_focus
    end

    # Detect whether the terminal reflows on its own
    # If it does we shouldn't do it.
    # Allow $ghoti_handle_reflow to override it.
    if not set -q ghoti_handle_reflow
        # VTE reflows the text itself, so us doing it inevitably races against it.
        # Guidance from the VTE developers is to let them repaint.
        if set -q VTE_VERSION
            # Same for alacritty
            or string match -q -- 'alacritty*' $TERM
            # Same for kitty
            or string match -q -- '*kitty' $TERM
            set -g ghoti_handle_reflow 0
        else if set -q KONSOLE_VERSION
            and test "$KONSOLE_VERSION" -ge 210400 2>/dev/null
            # Konsole since version 21.04(.00)
            # Note that this is optional, but since we have no way of detecting it
            # we go with the default, which is true.
            set -g ghoti_handle_reflow 0
        else
            set -g ghoti_handle_reflow 1
        end
    end

    function __ghoti_winch_handler --on-signal WINCH -d "Repaint screen when window changes size"
        if test "$ghoti_handle_reflow" = 1 2>/dev/null
            commandline -f repaint >/dev/null 2>/dev/null
        end
    end

    # Notify terminals when $PWD changes (issue #906).
    # VTE based terminals, Terminal.app, iTerm.app (TODO), foot, and kitty support this.
    if not set -q FISH_UNIT_TESTS_RUNNING
        and begin
            string match -q -- 'foot*' $TERM
            or string match -q -- 'xterm-kitty*' $TERM
            or test 0"$VTE_VERSION" -ge 3405
            or test "$TERM_PROGRAM" = Apple_Terminal && test (string match -r '\d+' 0"$TERM_PROGRAM_VERSION") -ge 309
            or test "$TERM_PROGRAM" = WezTerm
        end
        function __update_cwd_osc --on-variable PWD --description 'Notify capable terminals when $PWD changes'
            if status --is-command-substitution || set -q INSIDE_EMACS
                return
            end
            printf \e\]7\;file://%s%s\a $hostname (string escape --style=url $PWD)
        end
        __update_cwd_osc # Run once because we might have already inherited a PWD from an old tab
    end

    # Bump this whenever some code below needs to run once when upgrading to a new version.
    # The universal variable __ghoti_initialized is initialized in share/config.ghoti.
    set __ghoti_initialized 3400

    functions -e __ghoti_config_interactive
end
