function fish_vi_cursor -d 'Set cursor shape for different vi modes'
    # Check hard if we are in a supporting terminal.
    #
    # Challenges here are term-in-a-terms (emacs ansi-term does not support this, tmux does),
    # that we can only figure out if we are in konsole/iterm/vte via exported variables,
    # and ancient xterm versions.
    #
    # tmux defaults to $TERM = screen, but can do this if it is in a supporting terminal.
    # Unfortunately, we can only detect this via the exported variables, so we miss some cases.
    #
    # We will also miss some cases of terminal-stacking,
    # e.g. tmux started in suckless' st (no support) started in konsole.
    # But since tmux in konsole seems rather common and that case so uncommon,
    # we will just fail there (though it seems that tmux or st swallow it anyway).

    # If we're not interactive, there is effectively no bind mode.
    if not status is-interactive
        return
    end

    if set -q INSIDE_EMACS
        return
    end

    # vte-based terms set $TERM = xterm*, but only gained support relatively recently.
    # From https://bugzilla.gnome.org/show_bug.cgi?id=720821, it appears it was version 0.40.0
    if set -q VTE_VERSION
        and test "$VTE_VERSION" -lt 4000 2>/dev/null
        return
    end

    # We use the `tput` here just to see if terminfo thinks we can change the cursor.
    # We cannot use that sequence directly as it's not the correct one for konsole and iTerm,
    # and because we may want to change the cursor even though terminfo says we can't (tmux).
    if not tput Ss >/dev/null 2>/dev/null
        # Whitelist tmux...
        and not begin
            set -q TMUX
            # ...in a supporting term...
            and begin set -q KONSOLE_PROFILE_NAME
                or set -q ITERM_PROFILE
                or set -q VTE_VERSION # which version is already checked above
                or begin
                    set -q XTERM_VERSION
                    and test (string replace -r "XTerm\((\d+)\)" '$1' -- $XTERM_VERSION) -ge 280
                end
            end
            # .. unless an unsupporting terminal has been started in tmux inside a supporting one
            and begin string match -q "screen*" -- $TERM
                or string match -q "tmux*" -- $TERM
            end
        end
        and not string match -q "konsole*" -- $TERM
        or begin
            # TERM = xterm is special because plenty of things claim to be it, but aren't fully compatible
            # This includes old vte-terms (without $VTE_VERSION), old xterms (without $XTERM_VERSION or < 280)
            # and maybe other stuff.
            # This needs to be kept _at least_ as long as Ubuntu 14.04 is still a thing
            # because that ships a gnome-terminal without support and without $VTE_VERSION.
            string match -q 'xterm*' -- $TERM
            and not begin set -q KONSOLE_PROFILE_NAME
                or set -q ITERM_PROFILE
                or set -q VTE_VERSION # which version is already checked above
                # If $XTERM_VERSION is undefined, this will return 1 and print an error. Silence it.
                or test (string replace -r "XTerm\((\d+)\)" '$1' -- $XTERM_VERSION) -ge 280 2>/dev/null
            end
        end

        return
    end

    set -l terminal $argv[1]
    set -q terminal[1]
    or set terminal auto
    set -l uses_echo

    set -l function
    switch "$terminal"
        case auto
            if set -q KONSOLE_PROFILE_NAME
                set function __fish_cursor_konsole
                set uses_echo 1
            else if set -q ITERM_PROFILE
                set function __fish_cursor_1337
                set uses_echo 1
            else
                set function __fish_cursor_xterm
                set uses_echo 1
            end
        case konsole
            set function __fish_cursor_konsole
            set uses_echo 1
        case xterm
            set function __fish_cursor_xterm
            set uses_echo 1
    end

    set -l tmux_prefix
    set -l tmux_postfix
    if set -q TMUX
        and set -q uses_echo[1]
        set tmux_prefix echo -ne "'\ePtmux;\e'"
        set tmux_postfix echo -ne "'\e\\\\'"
    end

    set -q fish_cursor_unknown
    or set -g fish_cursor_unknown block blink

    echo "
          function fish_vi_cursor_handle --on-variable fish_bind_mode --on-event fish_postexec
              set -l varname fish_cursor_\$fish_bind_mode
              if not set -q \$varname
                set varname fish_cursor_unknown
              end
              $tmux_prefix
              $function \$\$varname
              $tmux_postfix
          end
         " | source

    echo "
          function fish_vi_cursor_handle_preexec --on-event fish_preexec
              set -l varname fish_cursor_default
              if not set -q \$varname
                set varname fish_cursor_unknown
              end
              $tmux_prefix
              $function \$\$varname
              $tmux_postfix
          end
         " | source
end

