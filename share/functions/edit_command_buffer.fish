# localization: tier1
function edit_command_buffer --description 'Edit the command buffer in an external editor'
    set -l tmpdir (__fish_mktemp_relative -d fish)
    or return 1
    set -l f $tmpdir/command-line.fish
    command touch $f
    or return 1

    set -l editor (__fish_anyeditor)
    or return 1

    set -l indented_lines (commandline -b | fish_indent --only-indent)
    string join -- \n $indented_lines >$f
    set -l offset (commandline --cursor)
    # compute cursor line/column
    set -l lines (commandline)\n
    set -l line 1
    while test $offset -ge (string length -- $lines[1])
        set offset (math $offset - (string length -- $lines[1]))
        set line (math $line + 1)
        set -e lines[1]
    end
    set -l indent 1 + (string length -- $indented_lines[$line]) - (string length -- $lines[1])
    set -l col (math $offset + 1 + $indent)

    set -l editor_basename (string match -r '[^/]+$' -- $editor[1])
    set -l wrapped_commands
    for wrap_target in (complete -- $editor_basename | string replace -rf '^complete [^/]+ --wraps (.+)$' '$1')
        set -l tmp
        string unescape -- $wrap_target | read -at tmp
        set -a wrapped_commands $tmp[1]
    end
    set -l found false
    set -l cursor_from_editor
    for editor_command in $editor_basename $wrapped_commands
        switch $editor_command
            case vi vim nvim
                if test $editor_command = vi && not set -l vi_version "$(vi --version 2>/dev/null)"
                    if printf %s $vi_version | grep -q BusyBox
                        break
                    end
                    set -a editor +{$line} $f
                    set found true
                    break
                end
                set cursor_from_editor (__fish_mktemp_relative fish-edit_command_buffer)
                or return
                # Map an insertion point to the preceding character in Vim.
                if test $col -gt 1
                    and test "$fish_bind_mode" = insert -o "$fish_key_bindings" = fish_default_key_bindings
                    set col $(math $col - 1)
                end
                # Vim's column is a byte index, while Fish counts characters.
                set -l cursor_report 'printf("%s %d %d %s", shellescape(bufname()), line("."), strchars(strpart(getline("."), 0, col(".") - 1)) + 1, shellescape(mode()))'
                set -a editor "+call cursor($line, byteidxcomp(getline($line), $col - 1) + 1)" $f \
                    '+au VimLeave * ++once call writefile(['$cursor_report'], "'$cursor_from_editor'")'
            case emacs emacsclient gedit
                set -a editor +$line:$col $f
            case kak
                set cursor_from_editor (__fish_mktemp_relative fish-edit_command_buffer)
                or return
                set -a editor +$line:$col $f -e "
                        hook -always -once global ClientClose %val{client} %{
                            echo -to-file $cursor_from_editor -quoting shell \
                                %val{buffile} %val{cursor_line} %val{cursor_column}
                        }
                    "
            case nano
                set -a editor +$line,$col $f
            case joe ee
                set -a editor +$line $f
            case code code-oss
                set -a editor --goto $f:$line:$col --wait
            case subl
                set -a editor $f:$line:$col --wait
            case micro
                set -a editor $f +$line:$col
            case helix hx
                set -a editor $f:$line:$col
            case '*'
                continue
        end
        set found true
        break
    end
    if not $found
        set -a editor $f
    end

    $editor
    set -l editor_status $status

    set -l raw_lines (command cat $f)
    set -l unindented_lines (string join -- \n $raw_lines | fish_indent --only-unindent)

    # Here we're checking the exit status of the editor.
    if test $editor_status -eq 0 -a -s $f
        # Set the command to the output of the edited command and move the cursor to the
        # end of the edited command.
        commandline -r -- $unindented_lines
        commandline -C 999999
    else
        echo
        echo (_ "Ignoring the output of your editor since its exit status was non-zero")
        echo (_ "or the file was empty")
    end
    # Apply the editor's cursor report even when discarding its edits.
    if set -q cursor_from_editor[1]
        eval set -l pos "$(command cat $cursor_from_editor)"
        if set -q pos[1] && test $pos[1] = $f -o $pos[1] -ef $f
            set -l line $pos[2]
            set -l length "$(string length -- "$unindented_lines[$line]")"
            set -l indent (math (string length -- "$raw_lines[$line]") - $length)
            set -l column (math $pos[3] - $indent)
            # Callers may use an exclusive cursor to prepare for entering
            # Insert mode when the editor succeeds.
            if test "$fish_bind_mode" = insert -o "$fish_key_bindings" = fish_default_key_bindings
                or test "$editor_status" -eq 0 -a "$fish_cursor_end_mode" = exclusive
                if test "$pos[4]" = n
                    # Resume insertion after the character under the Normal-mode cursor.
                    set column "$(math "min($column + 1, $length + 1)")"
                end
            else if test "$pos[4]" = i
                # Match Escape when returning to a character cursor.
                set column "$(math "max($column - 1, 1)")"
            end
            if not commandline --line $line 2>/dev/null
                commandline -f end-of-buffer
            else
                commandline --column $column 2>/dev/null || commandline -f end-of-line
            end
        end
        command rm $cursor_from_editor
    end
    command rm -r (path dirname $f)
    # We've probably opened something that messed with the screen.
    # A repaint seems in order.
    commandline -f repaint

    # The editor may have changed the terminal's cursor shape.
    functions -q fish_vi_cursor_handle; and fish_vi_cursor_handle
    return $editor_status
end
