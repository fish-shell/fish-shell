function __funced_md5
    if type -q md5sum
        # GNU systems
        echo (md5sum $argv[1] | string split ' ')[1]
        return 0
    else if type -q md5
        # BSD systems
        md5 -q $argv[1]
        return 0
    end
    return 1
end

function funced --description 'Edit function definition'
    set -l options 'h/help' 'e/editor=' 'i/interactive' 's/save'
    argparse -n funced --max-args=1 $options -- $argv
    or return

    if set -q _flag_help
        __fish_print_help funced
        return 0
    end

    if not set -q argv[1]
        printf (_ "%ls: Expected at least %d args, got only %d\n") funced 1 0
        return 1
    end

    set funcname $argv[1]

    # Check VISUAL first since theoretically EDITOR could be ed.
    set -l editor
    if set -q _flag_interactive
        set editor fish
    else if set -q _flag_editor
        set editor $_flag_editor
    else if set -q VISUAL
        set editor $VISUAL
    else if set -q EDITOR
        set editor $EDITOR
    else
        set editor fish
    end

    set -l init
    switch $funcname
        case '-*'
            set init function -- $funcname\n\nend
        case '*'
            set init function $funcname\n\nend
    end

    # Break editor up to get its first command (i.e. discard flags)
    set -l editor_cmd
    echo $editor | read -ta editor_cmd
    if not type -q -f "$editor_cmd[1]"
        echo (_ "funced: The value for \$EDITOR '$editor' could not be used because the command '$editor_cmd[1]' could not be found")
        set editor fish
    end

    if test "$editor" = fish
        if functions -q -- $funcname
            functions -- $funcname | fish_indent --no-indent | read -z init
        end

        set -l prompt 'printf "%s%s%s> " (set_color green) '$funcname' (set_color normal)'
        if read -p $prompt -c "$init" --shell cmd
            echo -n $cmd | fish_indent | read -lz cmd
            eval "$cmd"
        end
        if set -q _flag_save
            funcsave $funcname
        end
        return 0
    end

    # OS X (macOS) `mktemp` is rather restricted - no suffix, no way to automatically use TMPDIR.
    # Create a directory so we can use a ".fish" suffix for the file - makes editors pick up that
    # it's a fish file.
    set -q TMPDIR
    or set -l TMPDIR /tmp
    set -l tmpdir (mktemp -d $TMPDIR/fish.XXXXXX)
    set -l tmpname $tmpdir/$funcname.fish

    if functions -q -- $funcname
        functions -- $funcname >$tmpname
    else
        echo $init >$tmpname
    end

    # Repeatedly edit until it either parses successfully, or the user cancels
    # If the editor command itself fails, we assume the user cancelled or the file
    # could not be edited, and we do not try again
    while true
        set -l checksum (__funced_md5 "$tmpname")

        if not eval $editor $tmpname
            echo (_ "Editing failed or was cancelled")
        else
            # Verify the checksum (if present) to detect potential problems
            # with the editor command
            if set -q checksum[1]
                set -l new_checksum (__funced_md5 "$tmpname")
                if test "$new_checksum" = "$checksum"
                    echo (_ "Editor exited but the function was not modified")
                end
            end

            if not source $tmpname
                # Failed to source the function file. Prompt to try again.
                echo # add a line between the parse error and the prompt
                set -l repeat
                set -l prompt (_ 'Edit the file again\? [Y/n]')
                read -p "echo $prompt\  " response
                if test -z "$response"
                    or contains $response {Y,y}{E,e,}{S,s,}
                    continue
                else if not contains $response {N,n}{O,o,}
                    echo "I don't understand '$response', assuming 'Yes'"
                    sleep 2
                    continue
                end
                echo (_ "Cancelled function editing")
            else if set -q _flag_save
                funcsave $funcname
            end
        end
        break
    end

    set -l stat $status
    rm $tmpname >/dev/null
    and rmdir $tmpdir >/dev/null
    return $stat
end
