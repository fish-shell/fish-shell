function fish_vi_cursor -d 'Set cursor shape for different vi modes'
    # If we're not interactive, there is effectively no bind mode.
    if not status is-interactive
        return
    end

    # This is hard to test in expect, since the exact sequences depend on the environment.
    # Instead disable it.
    if set -q FISH_UNIT_TESTS_RUNNING
        return
    end

    # Emacs Makes All Cursors Suck
    if set -q INSIDE_EMACS
        return
    end

    # vte-based terms set $TERM = xterm*, but only gained support in 2015.
    # From https://bugzilla.gnome.org/show_bug.cgi?id=720821, it appears it was version 0.40.0
    if set -q VTE_VERSION
        and test "$VTE_VERSION" -lt 4000 2>/dev/null
        return
    end

    # Similarly, genuine XTerm can do it since v280.
    if set -q XTERM_VERSION
        and not test (string replace -r "XTerm\((\d+)\)" '$1' -- "$XTERM_VERSION") -ge 280 2>/dev/null
        return
    end

    # We need one of these terms.
    # It would be lovely if we could rely on terminfo, but:
    # - The "Ss" entry isn't a thing in macOS' old and crusty terminfo
    # - It is set for xterm, and everyone and their dog claims to be xterm
    #
    # So we just don't care about $TERM, unless it is one of the few terminals that actually have their own entry.
    #
    # Note: Previous versions also checked $TMUX, and made sure that then $TERM was screen* or tmux*.
    # We don't care, since we *cannot* handle term-in-a-terms 100% correctly.
    if not set -q KONSOLE_PROFILE_NAME
        and not set -q ITERM_PROFILE
        and not set -q VTE_VERSION # which version is already checked above
        and not set -q XTERM_VERSION
        and not string match -rq '^st(-.*)$' -- $TERM
        and not string match -q 'xterm-kitty*' -- $TERM
        and not string match -q 'rxvt*' -- $TERM
        return
    end

    # HACK: Explicitly disable on ITERM because of #3696, which is weirdness with multi-line prompts.
    # We allow an explicit "--force-iterm" as first argument to skip this.
    # It's recommended only if you don't use a multi-line prompt.
    if contains -- $argv[1] --force-iterm
        set -e argv[1]
    else if set -q ITERM_PROFILE
        return
    end

    set -l terminal $argv[1]
    set -q terminal[1]
    or set terminal auto

    set -l function
    switch "$terminal"
        case auto
            # TODO: Konsole as of 18.08 knows the xterm sequences,
            # but there's still bugs with it (as of konsole 19.04.0).
            #
            # If it is fixed, we'd have to read $KONSOLE_VERSION for a while,
            # though that was only introduced in 18.08 as well.
            if set -q KONSOLE_PROFILE_NAME
                set function __fish_cursor_konsole
            else if set -q ITERM_PROFILE
                set function __fish_cursor_1337
            else
                set function __fish_cursor_xterm
            end
        case konsole
            set function __fish_cursor_konsole
        case xterm
            set function __fish_cursor_xterm
    end

    set -l tmux_prefix
    set -l tmux_postfix
    if set -q TMUX
        set tmux_prefix echo -ne "'\ePtmux;\e'"
        set tmux_postfix echo -ne "'\e\\\\'"
    end

    set -q fish_cursor_unknown
    or set -g fish_cursor_unknown block blink

    echo "
          function fish_vi_cursor_handle --on-variable fish_bind_mode --on-event fish_postexec --on-event fish_focus_in
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

