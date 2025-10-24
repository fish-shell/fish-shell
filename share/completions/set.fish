#
# Completions for the 'set' builtin
#

#
# Various helper functions
#

function __fish_set_is_color -a foreground background -d 'Test if We are specifying a color value for the prompt'
    set -l cmd (commandline -pxc)
    set -e cmd[1]
    for i in $cmd
        switch $i
            case fish_color_search_match fish_color_selection fish_pager_color_selected_background
                $background
                return
            case 'fish_color_*' 'fish_pager_color_*'
                $foreground
                return

            case '-*'

            case '*'
                return 1
        end
    end
    return 1
end

function __fish_set_is_locale -d 'Test if We are specifying a locale value for the prompt'
    set -l cmd (commandline -pxc)
    set -e cmd[1]
    set -l locale_vars (__fish_locale_vars)
    for i in $cmd
        switch $i

            case $locale_vars
                return 0

            case '-*'
                continue

            case '*'
                return 1
        end
    end
    return 1
end

function __fish_complete_special_vars
    printf "%s\t%s\n" \
        PATH "list of dirs to look for commands in" \
        CDPATH "list of dirs under which that cd searches" \
        FISH_DEBUG "list of enabled debug categories" \
        FISH_DEBUG_OUTPUT "debug output path" \
        umask "current file creation mask" \
        fish_ambiguous_width "affects computed width of east asian chars" \
        fish_autosuggestion_enabled "set to 0 to turn autosuggestions off" \
        fish_cursor_end_mode "set to 'inclusive' to disallow moving the cursor beyond the command line end" \
        fish_cursor_selection_mode "set to 'inclusive' if selections should include the cursor" \
        fish_emoji_width "cols wide fish assumes emoji render as" \
        fish_escape_delay_ms "How long fish waits to distinguish escape and alt" \
        fish_greeting "The message to display at start (also a function)" \
        fish_handle_reflow "if fish should repaint prompt when the term resizes" \
        fish_history "The session id to store history under" \
        fish_key_bindings "name of function that sets binds" \
        fish_term24bit "set to 0 to use the color palette instead of true-colors" \
        fish_term256 "set to 0 to use the 16-color palette instead of 256" \
        fish_trace "Enables execution tracing (if set to non-empty value)" \
        fish_transient_prompt "set to 1 to re-run prompts before pushing them to scrollback" \
        fish_user_paths "A list of dirs to prepend to PATH"

end

#
# Completions
#

# Regular switches, set only accepts these before the variable name,
# so we need to test using "__fish_is_nth_token 1"

complete -c set -n "__fish_is_nth_token 1" -s e -l erase -d "Erase variable"
complete -c set -n "__fish_is_nth_token 1" -s x -l export -d "Export variable to subprocess"
complete -c set -n "__fish_is_nth_token 1" -s u -l unexport -d "Do not export variable to subprocess"
complete -c set -n "__fish_is_nth_token 1" -s f -l function -d "Make variable function-scoped"
complete -c set -n "__fish_is_nth_token 1" -s g -l global -d "Make variable scope global"
complete -c set -n "__fish_is_nth_token 1" -s l -l local -d "Make variable scope local"
complete -c set -n "__fish_is_nth_token 1" -s L -l long -d 'Do not truncate long lines'
complete -c set -n "__fish_is_nth_token 1" -s U -l universal -d "Share variable persistently across sessions"
complete -c set -n "__fish_is_nth_token 1" -s q -l query -d "Test if variable is defined"
complete -c set -n "__fish_is_nth_token 1" -s h -l help -d "Display help and exit"
complete -c set -n "__fish_is_nth_token 1" -s n -l names -d "List the names of the variables, but not their value"
complete -c set -n "__fish_is_nth_token 1" -s a -l append -d "Append value to a list"
complete -c set -n "__fish_is_nth_token 1" -s p -l prepend -d "Prepend value to a list"
complete -c set -n "__fish_is_nth_token 1" -s S -l show -d "Show variable"
complete -c set -n "__fish_is_nth_token 1" -l path -d "Make variable as a path variable"
complete -c set -n "__fish_is_nth_token 1" -l unpath -d "Make variable not as a path variable"
complete -c set -n "__fish_is_nth_token 1" -l no-event -d "Don't emit an event"

#TODO: add CPP code to generate list of read-only variables and exclude them from the following completions

# Complete using preexisting variable names
set -l maybe_filter_private_vars '
    string match (
        if string match -qr -- "^_" "$(commandline -t)": then
            echo "*"
        else
            echo -rv
            echo "^__"
        end
    )'
# We do not *filter* these by the given scope because you might want to set e.g. a global to shadow a universal.
complete -c set -n '__fish_is_nth_token 1; and not __fish_seen_argument -s e -l erase' -x -a "(set -U | $maybe_filter_private_vars | string replace ' ' \t'Universal Variable: ')"
complete -c set -n '__fish_is_nth_token 1; and not __fish_seen_argument -s e -l erase' -x -a "(set -g | $maybe_filter_private_vars | string replace -r '^((?:history|fish_killring) ).*' '\$1' | string replace ' ' \t'Global Variable: ')"
complete -c set -n '__fish_is_nth_token 1; and not __fish_seen_argument -s e -l erase' -x -a "(set -l | $maybe_filter_private_vars | string replace ' ' \t'Local Variable: ')"
# Complete some fish configuration variables even if they aren't set.
complete -c set -n '__fish_is_nth_token 1; and not __fish_seen_argument -s e -l erase' -x -a "(__fish_complete_special_vars)"

# Complete using preexisting variable names for `set --erase`
complete -c set -n '__fish_seen_argument -s e -l erase; and not __fish_seen_argument -s l -s U -s g -l local -l global -l Universal' -f -a "(set -U | string replace ' ' \tUniversal\ Variable:\ )"
complete -c set -n '__fish_seen_argument -s e -l erase; and not __fish_seen_argument -s l -s U -s g -l local -l global -l Universal' -f -a "(set -g | string replace ' ' \tGlobal\ Variable:\ )"
complete -c set -n '__fish_seen_argument -s e -l erase; and not __fish_seen_argument -s l -s U -s g -l local -l global -l Universal' -f -a "(set -l | string replace ' ' \tLocal\ Variable:\ )"
# Complete scope-specific variables for `set --erase`
complete -c set -n '__fish_seen_argument -s e -l erase; and __fish_seen_argument -s U -l universal' -f -a "(set -U | string replace ' ' \t'Universal Variable: ')"
complete -c set -n '__fish_seen_argument -s e -l erase; and __fish_seen_argument -s g -l global' -f -a "(set -g | string replace ' ' \t'Global Variable: ')"
complete -c set -n '__fish_seen_argument -s e -l erase; and __fish_seen_argument -s l -l local' -f -a "(set -l | string replace ' ' \t'Local Variable: ')"

# Color completions
complete -c set -n '__fish_set_is_color true false' -x -a '(set_color --print-colors)' -d 'text color'
complete -c set -n '__fish_set_is_color false true' -a '--background=(set_color --print-colors)'
complete -c set -n '__fish_set_is_color false true' -a '--underline-color=(set_color --print-colors)'
complete -c set -n '__fish_set_is_color true false' -a --bold -x
complete -c set -n '__fish_set_is_color true false' -a --dim -x
complete -c set -n '__fish_set_is_color true false' -a --italics -x
complete -c set -n '__fish_set_is_color true true' -a --reverse -x
complete -c set -n '__fish_set_is_color true false' -a --underline -x
complete -c set -n '__fish_set_is_color true false' -a--underline={double,curly,dotted,dashed} -x

# Locale completions
complete -c set -n '__fish_set_is_locale; and not __fish_seen_argument -s e -l erase' -x -a '(command -sq locale; and locale -a)' -d Locale
