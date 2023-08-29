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

    # If this variable is set, skip all checks
    if not set -q fish_vi_force_cursor

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
        if not set -q KONSOLE_PROFILE_NAME
            and not test -n "$KONSOLE_VERSION" -a "$KONSOLE_VERSION" -ge 200400 # konsole, but new.
            and not set -q ITERM_PROFILE
            and not set -q VTE_VERSION # which version is already checked above
            and not set -q WT_PROFILE_ID
            and not set -q XTERM_VERSION
            and not string match -q Apple_Terminal -- $TERM_PROGRAM
            and not string match -rq '^st(-.*)$' -- $TERM
            and not string match -q 'xterm-kitty*' -- $TERM
            and not string match -q 'rxvt*' -- $TERM
            and not string match -q 'alacritty*' -- $TERM
            and not string match -q 'foot*' -- $TERM
            and not begin
                set -q TMUX
                and string match -qr '^screen|^tmux' -- $TERM
            end
            return
        end
    end

    set -l terminal $argv[1]
    set -q terminal[1]
    or set terminal auto

    set -l function __fish_cursor_xterm

    set -q fish_cursor_unknown
    or set -g fish_cursor_unknown block

    echo "
          function fish_vi_cursor_handle --on-variable fish_bind_mode --on-event fish_postexec --on-event fish_focus_in
              set -l varname fish_cursor_\$fish_bind_mode
              if not set -q \$varname
                set varname fish_cursor_unknown
              end
              $function \$\$varname
          end
         " | source

    echo "
          function fish_vi_cursor_handle_preexec --on-event fish_preexec
              set -l varname fish_cursor_external
              if not set -q \$varname
                set varname fish_cursor_default
              end
              if not set -q \$varname
                set varname fish_cursor_unknown
              end
              $function \$\$varname
          end
         " | source
end
