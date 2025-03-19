function fish_vi_key_bindings --description 'vi-like key bindings for fish'
    set -l legacy_bind bind
    if contains -- -h $argv
        or contains -- --help $argv
        echo "Sorry but this function doesn't support -h or --help" >&2
        return 1
    end

    # Erase all bindings if not explicitly requested otherwise to
    # allow for hybrid bindings.
    # This needs to be checked here because if we are called again
    # via the variable handler the argument will be gone.
    set -l rebind true
    if test "$argv[1]" = --no-erase
        set rebind false
        set -e argv[1]
    else
        bind --erase --all --preset # clear earlier bindings, if any
    end

    # Allow just calling this function to correctly set the bindings.
    # Because it's a rather discoverable name, users will execute it
    # and without this would then have subtly broken bindings.
    if test "$fish_key_bindings" != fish_vi_key_bindings
        and test "$rebind" = true
        __fish_change_key_bindings fish_vi_key_bindings || return
    end

    set -l init_mode insert
    # These are only the special vi-style keys
    # not end/home, we share those.
    set -l eol_keys \$ g,\$
    set -l bol_keys \^ 0 g\^ _

    if contains -- $argv[1] insert default visual
        set init_mode $argv[1]
    else if set -q argv[1]
        # We should still go on so the bindings still get set.
        echo "Unknown argument $argv" >&2
    end

    # Inherit shared key bindings.
    # Do this first so vi-bindings win over default.
    for mode in insert default visual
        __fish_shared_key_bindings -s -M $mode
    end

    # Add a way to switch from insert to normal (command) mode.
    # Note if we are paging, we want to stay in insert mode
    # See #2871
    set -l on_escape '
        if commandline -P
            commandline -f cancel
        else
            set fish_bind_mode default
            if test (count (commandline --cut-at-cursor | tail -c2)) != 2
                commandline -f backward-char
            end
            commandline -f repaint-mode
        end
    '
    bind -s --preset -M insert escape $on_escape
    bind -s --preset -M insert ctrl-\[ $on_escape

    # Default (command) mode
    bind -s --preset :,q exit
    bind -s --preset -m insert ctrl-c clear-commandline repaint-mode
    bind -s --preset -M default h backward-char
    bind -s --preset -M default l forward-char
    bind -s --preset -m insert enter execute
    bind -s --preset -m insert ctrl-j execute
    bind -s --preset -m insert ctrl-m execute
    bind -s --preset -m insert o 'set fish_cursor_end_mode exclusive' insert-line-under repaint-mode
    bind -s --preset -m insert O 'set fish_cursor_end_mode exclusive' insert-line-over repaint-mode
    bind -s --preset -m insert i repaint-mode
    bind -s --preset -m insert I beginning-of-line repaint-mode
    bind -s --preset -m insert a 'set fish_cursor_end_mode exclusive' forward-single-char repaint-mode
    bind -s --preset -m insert A 'set fish_cursor_end_mode exclusive' end-of-line repaint-mode
    bind -s --preset -m visual v begin-selection repaint-mode

    bind -s --preset g,g beginning-of-buffer
    bind -s --preset G end-of-buffer

    for key in $eol_keys
        bind -s --preset $key end-of-line
    end
    for key in $bol_keys
        bind -s --preset $key beginning-of-line
    end

    bind -s --preset u undo
    bind -s --preset ctrl-r redo

    bind -s --preset [ history-token-search-backward
    bind -s --preset ] history-token-search-forward
    bind -s --preset -m insert / history-pager repaint-mode

    bind -s --preset k up-or-search
    bind -s --preset j down-or-search
    bind -s --preset b backward-word
    bind -s --preset B backward-bigword
    bind -s --preset g,e backward-word
    bind -s --preset g,E backward-bigword
    bind -s --preset w forward-word forward-single-char
    bind -s --preset W forward-bigword forward-single-char
    bind -s --preset e 'set fish_cursor_end_mode exclusive' forward-single-char forward-word backward-char 'set fish_cursor_end_mode inclusive'
    bind -s --preset E 'set fish_cursor_end_mode exclusive' forward-single-char forward-bigword backward-char 'set fish_cursor_end_mode inclusive'

    bind -s --preset -M insert ctrl-n accept-autosuggestion

    # Vi/Vim doesn't support these keys in insert mode but that seems silly so we do so anyway.
    bind -s --preset -M insert home beginning-of-line
    $legacy_bind -s --preset -M insert -k home beginning-of-line
    bind -s --preset -M default home beginning-of-line
    $legacy_bind -s --preset -M default -k home beginning-of-line
    bind -s --preset -M insert end end-of-line
    $legacy_bind -s --preset -M insert -k end end-of-line
    bind -s --preset -M default end end-of-line
    $legacy_bind -s --preset -M default -k end end-of-line

    # Vi moves the cursor back if, after deleting, it is at EOL.
    # To emulate that, move forward, then backward, which will be a NOP
    # if there is something to move forward to.
    bind -s --preset -M default x delete-char 'set fish_cursor_end_mode exclusive' forward-single-char backward-char 'set fish_cursor_end_mode inclusive'
    bind -s --preset -M default X backward-delete-char
    bind -s --preset -M insert delete delete-char forward-single-char backward-char
    $legacy_bind -s --preset -M insert -k dc delete-char forward-single-char backward-char
    bind -s --preset -M default delete delete-char 'set fish_cursor_end_mode exclusive' forward-single-char backward-char 'set fish_cursor_end_mode inclusive'
    $legacy_bind -s --preset -M default -k dc delete-char 'set fish_cursor_end_mode exclusive' forward-single-char backward-char 'set fish_cursor_end_mode inclusive'

    # Backspace deletes a char in insert mode, but not in normal/default mode.
    bind -s --preset -M insert backspace backward-delete-char
    bind -s --preset -M insert shift-backspace backward-delete-char
    $legacy_bind -s --preset -M insert -k backspace backward-delete-char
    bind -s --preset -M default backspace backward-char
    $legacy_bind -s --preset -M default -k backspace backward-char
    bind -s --preset -M insert ctrl-h backward-delete-char
    bind -s --preset -M default ctrl-h backward-char

    bind -s --preset d,d kill-whole-line
    bind -s --preset D kill-line
    bind -s --preset d,\$ kill-line
    bind -s --preset d,\^ backward-kill-line
    bind -s --preset d,0 backward-kill-line
    bind -s --preset d,w kill-word
    bind -s --preset d,W kill-bigword
    bind -s --preset d,i,w forward-single-char forward-single-char backward-word kill-word
    bind -s --preset d,i,W forward-single-char forward-single-char backward-bigword kill-bigword
    bind -s --preset d,a,w forward-single-char forward-single-char backward-word kill-word
    bind -s --preset d,a,W forward-single-char forward-single-char backward-bigword kill-bigword
    bind -s --preset d,e kill-word
    bind -s --preset d,E kill-bigword
    bind -s --preset d,b backward-kill-word
    bind -s --preset d,B backward-kill-bigword
    bind -s --preset d,g,e backward-kill-word
    bind -s --preset d,g,E backward-kill-bigword
    bind -s --preset d,f begin-selection forward-jump kill-selection end-selection
    bind -s --preset d,t begin-selection forward-jump backward-char kill-selection end-selection
    bind -s --preset d,F begin-selection backward-jump kill-selection end-selection
    bind -s --preset d,T begin-selection backward-jump forward-single-char kill-selection end-selection
    bind -s --preset d,h backward-char delete-char
    bind -s --preset d,l delete-char
    bind -s --preset d,i,b jump-till-matching-bracket and jump-till-matching-bracket and begin-selection jump-till-matching-bracket kill-selection end-selection
    bind -s --preset d,a,b jump-to-matching-bracket and jump-to-matching-bracket and begin-selection jump-to-matching-bracket kill-selection end-selection
    bind -s --preset d,i backward-jump-till and repeat-jump-reverse and begin-selection repeat-jump kill-selection end-selection
    bind -s --preset d,a backward-jump and repeat-jump-reverse and begin-selection repeat-jump kill-selection end-selection
    bind -s --preset 'd,;' begin-selection repeat-jump kill-selection end-selection
    bind -s --preset 'd,comma' begin-selection repeat-jump-reverse kill-selection end-selection

    bind -s --preset -m insert s delete-char repaint-mode
    bind -s --preset -m insert S kill-inner-line repaint-mode
    bind -s --preset -m insert c,c kill-inner-line repaint-mode
    bind -s --preset -m insert C kill-line repaint-mode
    bind -s --preset -m insert c,\$ kill-line repaint-mode
    bind -s --preset -m insert c,\^ backward-kill-line repaint-mode
    bind -s --preset -m insert c,0 backward-kill-line repaint-mode
    bind -s --preset -m insert c,w kill-word repaint-mode
    bind -s --preset -m insert c,W kill-bigword repaint-mode
    bind -s --preset -m insert c,i,w forward-single-char forward-single-char backward-word kill-word repaint-mode
    bind -s --preset -m insert c,i,W forward-single-char forward-single-char backward-bigword kill-bigword repaint-mode
    bind -s --preset -m insert c,a,w forward-single-char forward-single-char backward-word kill-word repaint-mode
    bind -s --preset -m insert c,a,W forward-single-char forward-single-char backward-bigword kill-bigword repaint-mode
    bind -s --preset -m insert c,e kill-word repaint-mode
    bind -s --preset -m insert c,E kill-bigword repaint-mode
    bind -s --preset -m insert c,b backward-kill-word repaint-mode
    bind -s --preset -m insert c,B backward-kill-bigword repaint-mode
    bind -s --preset -m insert c,g,e backward-kill-word repaint-mode
    bind -s --preset -m insert c,g,E backward-kill-bigword repaint-mode
    bind -s --preset -m insert c,f begin-selection forward-jump kill-selection end-selection repaint-mode
    bind -s --preset -m insert c,t begin-selection forward-jump backward-char kill-selection end-selection repaint-mode
    bind -s --preset -m insert c,F begin-selection backward-jump kill-selection end-selection repaint-mode
    bind -s --preset -m insert c,T begin-selection backward-jump forward-single-char kill-selection end-selection repaint-mode
    bind -s --preset -m insert c,h backward-char begin-selection kill-selection end-selection repaint-mode
    bind -s --preset -m insert c,l begin-selection kill-selection end-selection repaint-mode
    bind -s --preset -m insert c,i,b jump-till-matching-bracket and jump-till-matching-bracket and begin-selection jump-till-matching-bracket kill-selection end-selection
    bind -s --preset -m insert c,a,b jump-to-matching-bracket and jump-to-matching-bracket and begin-selection jump-to-matching-bracket kill-selection end-selection
    bind -s --preset -m insert c,i backward-jump-till and repeat-jump-reverse and begin-selection repeat-jump kill-selection end-selection repaint-mode
    bind -s --preset -m insert c,a backward-jump and repeat-jump-reverse and begin-selection repeat-jump kill-selection end-selection repaint-mode

    bind -s --preset '~' togglecase-char forward-single-char
    bind -s --preset g,u downcase-word
    bind -s --preset g,U upcase-word

    bind -s --preset J end-of-line delete-char
    bind -s --preset K 'man (commandline -t) 2>/dev/null; or echo -n \a'

    bind -s --preset yy kill-whole-line yank
    for seq in '",*,y,y' '",*,Y' '",+,y,y' '",+,Y'
        bind -s --preset $seq fish_clipboard_copy
    end
    bind -s --preset Y kill-whole-line yank
    bind -s --preset y,\$ kill-line yank
    bind -s --preset y,\^ backward-kill-line yank
    bind -s --preset y,0 backward-kill-line yank
    bind -s --preset y,w kill-word yank
    bind -s --preset y,W kill-bigword yank
    bind -s --preset y,i,w forward-single-char forward-single-char backward-word kill-word yank
    bind -s --preset y,i,W forward-single-char forward-single-char backward-bigword kill-bigword yank
    bind -s --preset y,a,w forward-single-char forward-single-char backward-word kill-word yank
    bind -s --preset y,a,W forward-single-char forward-single-char backward-bigword kill-bigword yank
    bind -s --preset y,e kill-word yank
    bind -s --preset y,E kill-bigword yank
    bind -s --preset y,b backward-kill-word yank
    bind -s --preset y,B backward-kill-bigword yank
    bind -s --preset y,g,e backward-kill-word yank
    bind -s --preset y,g,E backward-kill-bigword yank
    bind -s --preset y,f begin-selection forward-jump kill-selection yank end-selection
    bind -s --preset y,t begin-selection forward-jump-till kill-selection yank end-selection
    bind -s --preset y,F begin-selection backward-jump kill-selection yank end-selection
    bind -s --preset y,T begin-selection backward-jump-till kill-selection yank end-selection
    bind -s --preset y,h backward-char begin-selection kill-selection yank end-selection
    bind -s --preset y,l begin-selection kill-selection yank end-selection
    bind -s --preset y,i,b jump-till-matching-bracket and jump-till-matching-bracket and begin-selection jump-till-matching-bracket kill-selection yank end-selection
    bind -s --preset y,a,b jump-to-matching-bracket and jump-to-matching-bracket and begin-selection jump-to-matching-bracket kill-selection yank end-selection
    bind -s --preset y,i backward-jump-till and repeat-jump-reverse and begin-selection repeat-jump kill-selection yank end-selection
    bind -s --preset y,a backward-jump and repeat-jump-reverse and begin-selection repeat-jump kill-selection yank end-selection

    bind -s --preset % jump-to-matching-bracket
    bind -s --preset f forward-jump
    bind -s --preset F backward-jump
    bind -s --preset t forward-jump-till
    bind -s --preset T backward-jump-till
    bind -s --preset ';' repeat-jump
    bind -s --preset , repeat-jump-reverse

    # in emacs yank means paste
    # in vim p means paste *after* current character, so go forward a char before pasting
    # also in vim, P means paste *at* current position (like at '|' with cursor = line),
    # \ so there's no need to go back a char, just paste it without moving
    bind -s --preset p 'set -g fish_cursor_end_mode exclusive' forward-char 'set -g fish_cursor_end_modefish_cursor_end_modeinclusive' yank
    bind -s --preset P yank
    bind -s --preset g,p yank-pop

    # same vim 'pasting' note as upper
    bind -s --preset '",*,p' 'set -g fish_cursor_end_mode exclusive' forward-char 'set -g fish_cursor_end_mode inclusive' fish_clipboard_paste
    bind -s --preset '",*,P' fish_clipboard_paste
    bind -s --preset '",+,p' 'set -g fish_cursor_end_mode exclusive' forward-char 'set -g fish_cursor_end_mode inclusive' fish_clipboard_paste
    bind -s --preset '",+,P' fish_clipboard_paste

    #
    # Lowercase r, enters replace_one mode
    #
    bind -s --preset -m replace_one r repaint-mode
    bind -s --preset -M replace_one -m default '' delete-char self-insert backward-char repaint-mode
    bind -s --preset -M replace_one -m default enter 'commandline -f delete-char; commandline -i \n; commandline -f backward-char; commandline -f repaint-mode'
    bind -s --preset -M replace_one -m default ctrl-j 'commandline -f delete-char; commandline -i \n; commandline -f backward-char; commandline -f repaint-mode'
    bind -s --preset -M replace_one -m default ctrl-m 'commandline -f delete-char; commandline -i \n; commandline -f backward-char; commandline -f repaint-mode'
    bind -s --preset -M replace_one -m default escape cancel repaint-mode
    bind -s --preset -M replace_one -m default ctrl-\[ cancel repaint-mode

    #
    # Uppercase R, enters replace mode
    #
    bind -s --preset -m replace R repaint-mode
    bind -s --preset -M replace '' delete-char self-insert
    bind -s --preset -M replace -m insert enter execute repaint-mode
    bind -s --preset -M replace -m insert ctrl-j execute repaint-mode
    bind -s --preset -M replace -m insert ctrl-m execute repaint-mode
    bind -s --preset -M replace -m default escape cancel repaint-mode
    bind -s --preset -M replace -m default ctrl-\[ cancel repaint-mode
    # in vim (and maybe in vi), <BS> deletes the changes
    # but this binding just move cursor backward, not delete the changes
    bind -s --preset -M replace backspace backward-char
    bind -s --preset -M replace shift-backspace backward-char
    $legacy_bind -s --preset -M replace -k backspace backward-char

    #
    # visual mode
    #
    bind -s --preset -M visual h backward-char
    bind -s --preset -M visual l forward-char

    bind -s --preset -M visual k up-line
    bind -s --preset -M visual j down-line

    bind -s --preset -M visual b backward-word
    bind -s --preset -M visual B backward-bigword
    bind -s --preset -M visual g,e backward-word
    bind -s --preset -M visual g,E backward-bigword
    bind -s --preset -M visual w forward-word
    bind -s --preset -M visual W forward-bigword
    bind -s --preset -M visual e 'set fish_cursor_end_mode exclusive' forward-single-char forward-word backward-char 'set fish_cursor_end_mode inclusive'
    bind -s --preset -M visual E 'set fish_cursor_end_mode exclusive' forward-single-char forward-bigword backward-char 'set fish_cursor_end_mode inclusive'
    bind -s --preset -M visual o swap-selection-start-stop repaint-mode

    bind -s --preset -M visual % jump-to-matching-bracket
    bind -s --preset -M visual f forward-jump
    bind -s --preset -M visual t forward-jump-till
    bind -s --preset -M visual F backward-jump
    bind -s --preset -M visual T backward-jump-till
    bind -s --preset -M visual ';' repeat-jump
    bind -s --preset -M visual , repeat-jump-reverse

    for key in $eol_keys
        bind -s --preset -M visual $key end-of-line
    end
    for key in $bol_keys
        bind -s --preset -M visual $key beginning-of-line
    end

    bind -s --preset -M visual -m default v end-selection repaint-mode
    bind -s --preset -M visual -m insert i end-selection repaint-mode
    bind -s --preset -M visual -m insert I end-selection beginning-of-line repaint-mode
    bind -s --preset -M visual -m insert c kill-selection end-selection repaint-mode
    bind -s --preset -M visual -m insert s kill-selection end-selection repaint-mode
    bind -s --preset -M visual -m default d kill-selection end-selection backward-char repaint-mode
    bind -s --preset -M visual -m default x kill-selection end-selection repaint-mode
    bind -s --preset -M visual -m default X kill-whole-line end-selection repaint-mode
    bind -s --preset -M visual -m default y kill-selection yank end-selection repaint-mode
    bind -s --preset -M visual -m default '",*,y' "fish_clipboard_copy; commandline -f end-selection repaint-mode"
    bind -s --preset -M visual -m default '",+,y' "fish_clipboard_copy; commandline -f end-selection repaint-mode"
    bind -s --preset -M visual -m default '~' togglecase-selection end-selection repaint-mode
    bind -s --preset -M visual -m default g,U togglecase-selection end-selection repaint-mode

    bind -s --preset -M visual -m default ctrl-c end-selection repaint-mode
    bind -s --preset -M visual -m default escape end-selection repaint-mode
    bind -s --preset -M visual -m default ctrl-\[ end-selection repaint-mode

    # Make it easy to turn an unexecuted command into a comment in the shell history. Also, remove
    # the commenting chars so the command can be further edited then executed.
    bind -s --preset -M default \# __fish_toggle_comment_commandline
    bind -s --preset -M visual \# __fish_toggle_comment_commandline
    bind -s --preset -M replace \# __fish_toggle_comment_commandline

    # Set the cursor shape
    # After executing once, this will have defined functions listening for the variable.
    # Therefore it needs to be before setting fish_bind_mode.
    fish_vi_cursor
    set -g fish_cursor_selection_mode inclusive
    function __fish_vi_key_bindings_on_mode_change --on-variable fish_bind_mode
        switch $fish_bind_mode
            case insert replace
                set -g fish_cursor_end_mode exclusive
            case '*'
                set -g fish_cursor_end_mode inclusive
        end
    end
    function __fish_vi_key_bindings_remove_handlers --on-variable __fish_active_key_bindings
        functions --erase __fish_vi_key_bindings_remove_handlers
        functions --erase __fish_vi_key_bindings_on_mode_change
        functions --erase fish_vi_cursor_handle
        functions --erase fish_vi_cursor_handle_preexec
        set -e -g fish_cursor_end_mode
        set -e -g fish_cursor_selection_mode
    end

    set fish_bind_mode $init_mode
end
