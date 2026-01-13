function fish_vi_dec
    fish_vi_inc_dec dec
end

function fish_vi_inc
    fish_vi_inc_dec inc
end

function fish_vi_arg_digit --description 'Accumulate a digit for the next command'
    set -g __fish_vi_count "$__fish_vi_count$argv[1]"
end

function fish_vi_run_count --description 'Run a command $__fish_vi_count times'
    set -l count (__fish_vi_consume_count __fish_vi_count)

    for i in (seq $count)
        if functions -q -- $argv[1]
            $argv
        else
            commandline -f $argv
        end
    end
end

function fish_vi_start_operator
    set -g __fish_vi_operator $argv[1]
    set -g __fish_vi_start_count (__fish_vi_consume_count __fish_vi_count)
    set fish_bind_mode operator
    commandline -f repaint-mode
end

function fish_vi_operator_cancel
    set -g __fish_vi_operator
    set -g __fish_vi_start_count
    set -g __fish_vi_count
    set fish_bind_mode default
    commandline -f repaint-mode
end

function __fish_vi_consume_count -a varname
    set -l effective_count $$varname
    if test -z "$effective_count"
        set effective_count 1
    end
    set -g $varname
    echo $effective_count
end

function fish_vi_exec_motion
    argparse linewise -- $argv
    or return

    set -l motion $argv
    set -l total (math (__fish_vi_consume_count __fish_vi_start_count) \* (__fish_vi_consume_count __fish_vi_count))

    set fish_bind_mode default

    if set -ql _flag_linewise
        switch $__fish_vi_operator
            case delete
                for i in (seq $total)
                    commandline -f kill-whole-line
                end
            case change
                for i in (seq $total)
                    commandline -f kill-inner-line
                end
                set fish_bind_mode insert
            case yank
                for i in (seq $total)
                    commandline -f kill-whole-line yank
                end
            case swap-case
                # Not implemented yet
                return
        end
    else
        set -l use_selection true
        set -l swap_case_hack
        switch $motion
            case forward-word-vi forward-bigword-vi
                if test $__fish_vi_operator = swap-case
                    set swap_case_hack (string replace -r -- '^forward-((?:big)?word)-vi$' '$1' $motion)
                else
                    set use_selection false
                    set motion (string replace -- forward kill $motion)
                end
        end
        if $use_selection
            commandline -f begin-selection
        else
            commandline -f begin-undo-group
        end
        switch $__fish_vi_operator
            case delete
                for i in (seq $total)
                    commandline -f $motion
                end
                if $use_selection
                    commandline -f kill-selection
                end
            case change
                for i in (seq $total)
                    commandline -f $motion
                end
                if $use_selection
                    commandline -f kill-selection
                end
                set fish_bind_mode insert
            case yank
                for i in (seq $total)
                    commandline -f $motion
                end
                if $use_selection
                    commandline -f kill-selection
                end
                commandline -f yank
            case swap-case
                for i in (seq $total)
                    commandline -f $motion
                end
                if set -q swap_case_hack[1]
                    set -l word $swap_case_hack
                    commandline -f \
                        backward-$word \
                        forward-$word-end \
                        togglecase-selection \
                        backward-$word \
                        forward-$word-vi
                else
                    commandline -f togglecase-selection
                end
        end
        if $use_selection
            commandline -f end-selection
        else
            commandline -f end-undo-group
        end
    end
    commandline -f repaint-mode
    set -g __fish_vi_operator
end

# TODO: Currently we do not support hexadecimal and octal values.
function fish_vi_inc_dec --description 'increment or decrement the number below the cursor'
    # The cursor is zero based, but all string functions assume 1 to be
    # the lowest index. Adjust accordingly.
    set --local cursor (math -- (commandline --cursor) + 1)
    set --local line (commandline --current-buffer | string collect)

    set --local candidate (string sub --start $cursor -- $line | string collect)
    if set --local just_found (string match --regex '^-?[0-9]+' -- $candidate)
        # Search from the current cursor position backwards for as long as we
        # can identify a valid number.
        set --function found $just_found
        set --function found_at $cursor
        set --local end (math -- $cursor + (string length -- $found) - 1)

        set i (math -- $cursor - 1)
        while [ $i -ge 1 ]
            set candidate (string sub --start $i --end $end -- $line)
            if set just_found (string match --regex '^-?[0-9]+$' -- $candidate)
                set found $just_found
                set found_at $i
                # We found a candidate, but continue to make sure that we captured
                # the complete number and not just part of it.
            else
                # We have already found a number earlier. Work with that.
                break
            end

            set i (math -- $i - 1)
        end
    else
        # We didn't find a match below the cursor. Mirror Vim behavior by
        # checking ahead as well.
        for i in (seq (math -- $cursor + 1) (math -- (string length -- $line) - 1))
            set candidate (string sub --start $i -- $line | string collect)

            if set just_found (string match --regex '^-?[0-9]+' -- $candidate)
                set found $just_found
                set found_at $i
                break
            end
        end

        if [ -z "$found" ]
            return
        end
    end

    if [ $argv = inc ]
        set number (math -- $found + 1)
    else if [ $argv = dec ]
        set number (math -- $found - 1)
    end

    set --local number_abs (string trim --left --chars=- -- $number)
    set --local signed $status
    set --local found_abs (string trim --left --chars=- -- $found)
    set number (string pad --char 0 --width (string length -- $found_abs) -- $number_abs)
    if test $signed -eq 0
        set number "-$number"
    end

    # `string sub` may bitch about `--end` being zero if `found_at` is 1.
    # So ignore errors here...
    set --local before (string sub --end (math -- $found_at - 1) -- $line 2> /dev/null | string collect)
    set --local after (string sub --start (math -- $found_at + (string length -- $found)) -- $line | string collect)
    commandline --replace -- "$before$number$after"
    # Need to subtract two here because 1) cursor is zero based 2)
    # `found_at` is the index of the first character of the match, but we
    # want the one before that.
    commandline --cursor -- (math -- $found_at + (string length -- $number) - 2)
    commandline --function -- repaint
end

function fish_vi_key_bindings --description 'vi-like key bindings for fish'
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
        __fish_change_key_bindings fish_vi_key_bindings
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

    bind -s --preset -M default escape 'set -g __fish_vi_count'
    bind -s --preset -M default ctrl-\[ 'set -g __fish_vi_count'

    for i in (seq 1 9)
        bind -s --preset -M default $i "fish_vi_arg_digit $i"
    end
    # 0 is special: it is 'beginning-of-line' unless we are already counting (e.g. 10)
    bind -s --preset -M default 0 "if test -n \"\$__fish_vi_count\"; fish_vi_arg_digit 0; else; commandline -f beginning-of-line; end"

    # --- Movement with Count Support ---
    bind -s --preset -M default h 'fish_vi_run_count backward-char'
    bind -s --preset -M default l 'fish_vi_run_count forward-char'

    bind -s --preset -M default k 'fish_vi_run_count up-or-search'
    bind -s --preset -M default j 'fish_vi_run_count down-or-search'

    bind -s --preset -M default b 'fish_vi_run_count backward-word'
    bind -s --preset -M default B 'fish_vi_run_count backward-bigword'
    bind -s --preset -M default g,e 'fish_vi_run_count backward-word-end'
    bind -s --preset -M default g,E 'fish_vi_run_count backward-bigword-end'

    bind -s --preset -M default w 'fish_vi_run_count forward-word-vi'
    bind -s --preset -M default W 'fish_vi_run_count forward-bigword-vi'

    bind -s --preset -M default e 'fish_vi_run_count forward-word-end'
    bind -s --preset -M default E 'fish_vi_run_count forward-bigword-end'

    bind -s --preset -M default x 'fish_vi_run_count delete-char'
    bind -s --preset -M default X 'fish_vi_run_count backward-delete-char'

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
    # Note: 0 is handled in the numeric section above
    for key in \^ g\^ _
        bind -s --preset $key beginning-of-line
    end

    bind -s --preset u undo
    bind -s --preset ctrl-r redo

    bind -s --preset [ history-token-search-backward
    bind -s --preset ] history-token-search-forward
    bind -s --preset -m insert / history-pager repaint-mode

    __fish_per_os_bind --preset $argv ctrl-right forward-token forward-word-vi
    # ctrl-left is same as emacs mode

    bind -s --preset -M insert ctrl-n accept-autosuggestion

    # Vi/Vim doesn't support these keys in insert mode but that seems silly so we do so anyway.
    bind -s --preset -M insert home beginning-of-line
    bind -s --preset -M default home beginning-of-line
    bind -s --preset -M insert end end-of-line
    bind -s --preset -M default end end-of-line

    bind -s --preset -M insert delete delete-char
    bind -s --preset -M default delete delete-char

    # Backspace deletes a char in insert mode, but not in normal/default mode.
    bind -s --preset -M insert backspace backward-delete-char
    bind -s --preset -M insert shift-backspace backward-delete-char
    bind -s --preset -M default backspace backward-char
    bind -s --preset -M insert ctrl-h backward-delete-char
    bind -s --preset -M default ctrl-h backward-char

    # Operators & Operator Mode
    bind -s --preset -M default d 'fish_vi_start_operator delete'
    bind -s --preset -M default c 'fish_vi_start_operator change'
    bind -s --preset -M default y 'fish_vi_start_operator yank'
    bind -s --preset -M default g,\~ 'fish_vi_start_operator swap-case'

    bind -s --preset -M operator escape fish_vi_operator_cancel
    bind -s --preset -M operator ctrl-\[ fish_vi_operator_cancel

    for i in (seq 1 9)
        bind -s --preset -M operator $i "fish_vi_arg_digit $i"
    end
    bind -s --preset -M operator 0 "if test -n \"\$__fish_vi_count\"; fish_vi_arg_digit 0; else; fish_vi_exec_motion beginning-of-line; end"

    bind -s --preset -M operator h 'fish_vi_exec_motion backward-char'
    bind -s --preset -M operator l 'fish_vi_exec_motion forward-char'
    bind -s --preset -M operator k 'fish_vi_exec_motion up-line'
    bind -s --preset -M operator j 'fish_vi_exec_motion down-line'
    bind -s --preset -M operator b 'fish_vi_exec_motion backward-word'
    bind -s --preset -M operator B 'fish_vi_exec_motion backward-bigword'
    bind -s --preset -M operator g,e 'fish_vi_exec_motion backward-word-end'
    bind -s --preset -M operator g,E 'fish_vi_exec_motion backward-bigword-end'
    bind -s --preset -M operator w 'fish_vi_exec_motion forward-word-vi'
    bind -s --preset -M operator W 'fish_vi_exec_motion forward-bigword-vi'
    bind -s --preset -M operator e 'fish_vi_exec_motion forward-word-end'
    bind -s --preset -M operator E 'fish_vi_exec_motion forward-bigword-end'

    bind -s --preset -M operator 0 'fish_vi_exec_motion beginning-of-line'
    bind -s --preset -M operator \^ 'fish_vi_exec_motion beginning-of-line'
    bind -s --preset -M operator \$ 'fish_vi_exec_motion end-of-line'

    bind -s --preset -M operator f 'fish_vi_exec_motion forward-jump'
    bind -s --preset -M operator F 'fish_vi_exec_motion backward-jump'
    bind -s --preset -M operator t 'fish_vi_exec_motion forward-jump-till'
    bind -s --preset -M operator T 'fish_vi_exec_motion backward-jump-till'
    bind -s --preset -M operator ';' 'fish_vi_exec_motion repeat-jump'
    bind -s --preset -M operator , 'fish_vi_exec_motion repeat-jump-reverse'

    bind -s --preset -M operator d 'fish_vi_exec_motion --linewise'
    bind -s --preset -M operator c 'fish_vi_exec_motion --linewise'
    bind -s --preset -M operator y 'fish_vi_exec_motion --linewise'
    bind -s --preset -M operator \~ 'fish_vi_exec_motion --linewise'

    bind -s --preset D kill-line
    bind -s --preset d,\$ kill-line
    bind -s --preset d,\^ backward-kill-line
    bind -s --preset d,0 backward-kill-line

    bind -s --preset d,i,w kill-inner-word
    bind -s --preset d,i,W kill-inner-bigword
    bind -s --preset d,a,w kill-a-word
    bind -s --preset d,a,W kill-a-bigword
    bind -s --preset d,i,b jump-till-matching-bracket and jump-till-matching-bracket and begin-selection jump-till-matching-bracket kill-selection end-selection
    bind -s --preset d,a,b jump-to-matching-bracket and jump-to-matching-bracket and begin-selection jump-to-matching-bracket kill-selection end-selection
    bind -s --preset d,i backward-jump-till and repeat-jump-reverse and begin-selection repeat-jump kill-selection end-selection
    bind -s --preset d,a backward-jump and repeat-jump-reverse and begin-selection repeat-jump kill-selection end-selection
    bind -s --preset 'd,;' begin-selection repeat-jump kill-selection end-selection
    bind -s --preset 'd,comma' begin-selection repeat-jump-reverse kill-selection end-selection

    bind -s --preset -m insert s delete-char repaint-mode
    bind -s --preset -m insert S kill-inner-line repaint-mode
    bind -s --preset -m insert C kill-line repaint-mode
    bind -s --preset -m insert c,\$ kill-line repaint-mode
    bind -s --preset -m insert c,\^ backward-kill-line repaint-mode
    bind -s --preset -m insert c,0 backward-kill-line repaint-mode

    bind -s --preset -m insert c,i,w kill-inner-word repaint-mode
    bind -s --preset -m insert c,i,W kill-inner-bigword repaint-mode
    bind -s --preset -m insert c,a,w kill-a-word repaint-mode
    bind -s --preset -m insert c,a,W kill-a-bigword repaint-mode
    bind -s --preset -m insert c,i,b jump-till-matching-bracket and jump-till-matching-bracket and begin-selection jump-till-matching-bracket kill-selection end-selection
    bind -s --preset -m insert c,a,b jump-to-matching-bracket and jump-to-matching-bracket and begin-selection jump-to-matching-bracket kill-selection end-selection
    bind -s --preset -m insert c,i backward-jump-till and repeat-jump-reverse and begin-selection repeat-jump kill-selection end-selection repaint-mode
    bind -s --preset -m insert c,a backward-jump and repeat-jump-reverse and begin-selection repeat-jump kill-selection end-selection repaint-mode

    bind -s --preset \~ togglecase-char forward-single-char
    bind -s --preset g,u downcase-word
    bind -s --preset g,U upcase-word

    bind -s --preset J end-of-line delete-char
    bind -s --preset K 'man (commandline -t) 2>/dev/null; or echo -n \a'

    # yy handled by operator mode
    for seq in '",*,y,y' '",*,Y' '",+,y,y' '",+,Y'
        bind -s --preset $seq fish_clipboard_copy
    end
    bind -s --preset Y kill-whole-line yank
    bind -s --preset y,\$ kill-line yank
    bind -s --preset y,\^ backward-kill-line yank
    bind -s --preset y,0 backward-kill-line yank
    bind -s --preset y,i,w kill-inner-word yank
    bind -s --preset y,i,W kill-inner-bigword yank
    bind -s --preset y,a,w kill-a-word yank
    bind -s --preset y,a,W kill-a-bigword yank
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
    bind -s --preset p 'set -g fish_cursor_end_mode exclusive' forward-char 'set -g fish_cursor_end_mode inclusive' yank
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
    bind -s --preset -M replace_one -m default '' 'set -g fish_cursor_end_mode exclusive' delete-char self-insert backward-char repaint-mode 'set -g fish_cursor_end_mode inclusive'
    bind -s --preset -M replace_one -m default enter 'set -g fish_cursor_end_mode exclusive' 'commandline -f delete-char; commandline -i \n; commandline -f backward-char' repaint-mode 'set -g fish_cursor_end_mode inclusive'
    bind -s --preset -M replace_one -m default ctrl-j 'set -g fish_cursor_end_mode exclusive' 'commandline -f delete-char; commandline -i \n; commandline -f backward-char' repaint-mode 'set -g fish_cursor_end_mode inclusive'
    bind -s --preset -M replace_one -m default ctrl-m 'set -g fish_cursor_end_mode exclusive' 'commandline -f delete-char; commandline -i \n; commandline -f backward-char' repaint-mode 'set -g fish_cursor_end_mode inclusive'
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

    #
    # Increment or decrement number under the cursor with ctrl+x ctrl+a
    #
    bind -s --preset -M default ctrl-a fish_vi_inc
    bind -s --preset -M default ctrl-x fish_vi_dec

    #
    # visual mode
    #
    bind -s --preset -M visual h backward-char
    bind -s --preset -M visual l forward-char

    bind -s --preset -M visual k up-line
    bind -s --preset -M visual j down-line

    bind -s --preset -M visual b backward-word
    bind -s --preset -M visual B backward-bigword
    bind -s --preset -M visual g,e backward-word-end
    bind -s --preset -M visual g,E backward-bigword-end
    bind -s --preset -M visual w forward-word-vi
    bind -s --preset -M visual W forward-bigword-vi
    bind -s --preset -M visual e forward-word-end
    bind -s --preset -M visual E forward-bigword-end
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
    bind -s --preset -M visual -m default \~ togglecase-selection end-selection repaint-mode
    bind -s --preset -M visual -m default g,u downcase-selection end-selection repaint-mode
    bind -s --preset -M visual -m default g,U upcase-selection end-selection repaint-mode

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
        if type -q __fish_vi_cursor
            __fish_vi_cursor fish_cursor_default
        end
        functions --erase fish_vi_cursor_handle
        functions --erase fish_vi_cursor_handle_preexec
        set -e -g fish_cursor_end_mode
        set -e -g fish_cursor_selection_mode
    end

    set fish_bind_mode $init_mode
end
