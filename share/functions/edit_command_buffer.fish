function edit_command_buffer --description 'Edit the command buffer in an external editor'
    set -l f (mktemp)
    or return 1
    if set -q f[1]
        command mv $f $f.fish
        set f $f.fish
    else
        # We should never execute this block but better to be paranoid.
        if set -q TMPDIR
            set f $TMPDIR/fish.$fish_pid.fish
        else
            set f /tmp/fish.$fish_pid.fish
        end
        command touch $f
        or return 1
    end

    set -l editor (__fish_anyeditor)
    or return 1

    set -l indented_lines (commandline -b | __fish_indent --only-indent)
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
                set cursor_from_editor (mktemp)
                set -a editor +$line "+norm! $col|" $f \
                    '+au VimLeave * ++once call writefile([printf("%s %s %s", shellescape(bufname()), line("."), col("."))], "'$cursor_from_editor'")'
            case emacs emacsclient gedit
                set -a editor +$line:$col $f
            case kak
                set cursor_from_editor (mktemp)
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

    set -l raw_lines (command cat $f)
    set -l unindented_lines (string join -- \n $raw_lines | __fish_indent --only-unindent)

    # Here we're checking the exit status of the editor.
    if test $status -eq 0 -a -s $f
        # Set the command to the output of the edited command and move the cursor to the
        # end of the edited command.
        commandline -r -- $unindented_lines
        commandline -C 999999
    else
        echo
        echo (_ "Ignoring the output of your editor since its exit status was non-zero")
        echo (_ "or the file was empty")
    end
    if set -q cursor_from_editor[1]
        eval set -l pos "$(cat $cursor_from_editor)"
        if set -q pos[1] && test $pos[1] = $f
            set -l line $pos[2]
            set -l indent (math (string length -- "$raw_lines[$line]") - (string length -- "$unindented_lines[$line]"))
            set -l column (math $pos[3] - $indent)
            if not commandline --line $line 2>/dev/null
                commandline -f end-of-buffer
            else
                commandline --column $column 2>/dev/null || commandline -f end-of-line
            end
        end
        command rm $cursor_from_editor
    end
    command rm $f
    # We've probably opened something that messed with the screen.
    # A repaint seems in order.
    commandline -f repaint
end
