function funced --description 'Edit function definition'
    set -l options h/help 'e/editor=' i/interactive s/save
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

    set -l funcname $argv[1]

    # Check VISUAL first since theoretically EDITOR could be ed.
    set -l editor
    if set -q _flag_interactive
        set editor fish
    else if set -q _flag_editor
        echo $_flag_editor | read -at editor
    else if set -q VISUAL
        echo $VISUAL | read -at editor
    else if set -q EDITOR
        echo $EDITOR | read -at editor
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

    if not type -q -f "$editor[1]"
        echo (_ "funced: The value for \$EDITOR '$editor' could not be used because the command '$editor[1]' could not be found") >&2
        set editor fish
    end

    if test "$editor" = fish
        if functions -q -- $funcname
            functions --no-details -- $funcname | __fish_indent --only-unindent | __fish_indent --no-indent | read -z init
        end

        set -l prompt 'printf "%s%s%s> " (set_color green) '$funcname' (set_color normal)'
        if read -p $prompt -c "$init" --shell cmd
            echo -n $cmd | __fish_indent --only-unindent | read -lz cmd
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
    or return 1
    set -l tmpname $tmpdir/$funcname.fish

    set -l writepath

    if not functions -q -- $funcname
        echo $init >$tmpname
    else if functions --details -- $funcname | string match --invert --quiet --regex '^(?:-|stdin)$'
        set writepath (functions --details -- $funcname)
        # Use cat here rather than cp to avoid copying permissions
        cat "$writepath" >$tmpname
    else
        functions -- $funcname >$tmpname
    end

    # Repeatedly edit until it either parses successfully, or the user cancels
    # If the editor command itself fails, we assume the user cancelled or the file
    # could not be edited, and we do not try again
    while true
        set -l checksum (__fish_md5 "$tmpname")

        if not $editor $tmpname
            echo (_ "Editing failed or was cancelled")
        else
            # Verify the checksum (if present) to detect potential problems
            # with the editor command
            if set -q checksum[1]
                set -l new_checksum (__fish_md5 "$tmpname")
                if test "$new_checksum" = "$checksum"
                    echo (_ "Editor exited but the function was not modified")
                    echo (_ "If the editor is still running, check if it waits for completion, maybe a '--wait' option?")
                    # Source but don't save an unmodified file.
                    # (Source in case the file changed externally since we first loaded it.)
                    if set -q writepath[1]
                        source "$writepath"
                    end
                    break
                end
            end

            if not source <$tmpname
                # Failed to source the function file. Prompt to try again.
                echo # add a line between the parse error and the prompt
                set -l repeat
                set -l prompt (_ 'Edit the file again? [Y/n]')
                read -P "$prompt " response
                if test -z "$response"
                    or contains $response {Y,y}{E,e,}{S,s,}
                    continue
                else if not contains $response {N,n}{O,o,}
                    echo "I don't understand '$response', assuming 'Yes'"
                    sleep 2
                    continue
                end
                echo (_ "Cancelled function editing")
            else if test -n "$writepath"
                if not set -q _flag_save
                    echo (_ "Warning: the file containing this function has not been saved. Changes may be lost when fish is closed.")
                    set -l prompt (printf (_ 'Save function to %s? [Y/n]') "$writepath")
                    read --prompt-str "$prompt " response
                    if test -z "$response"
                        or contains $response {Y,y}{E,e,}{S,s,}
                        set _flag_save 1
                    else if not contains $response {N,n}{O,o,}
                        echo "I don't understand '$response', assuming 'Yes'"
                        set _flag_save 1
                    end
                end
                if set -q _flag_save
                    # try to write the file back
                    # cp preserves existing permissions, though it might overwrite the owner
                    if cp $tmpname "$writepath" 2>&1
                        printf (_ "Function saved to %s") "$writepath"
                        echo
                        # read it back again - this ensures that the output of `functions --details` is correct
                        source "$writepath"
                    else
                        echo (_ "Saving to original location failed; saving to user configuration instead.")
                        set writepath $__fish_config_dir/functions/(path basename "$writepath")
                        if cp $tmpname "$writepath"
                            printf (_ "Function saved to %s") "$writepath"
                            echo
                            # read it back again - this ensures that the output of `functions --details` is correct
                            source "$writepath"
                        else
                            echo (_ "Saving to user configuration failed. Changes may be lost when fish is closed.")
                        end
                    end
                end
            else if set -q _flag_save
                funcsave $funcname
            else
                printf (_ "Run funcsave %s to save this function to the configuration directory.") $funcname
                echo
            end
        end
        break
    end

    set -l stat $status
    command rm $tmpname >/dev/null
    and rmdir $tmpdir >/dev/null
    return $stat
end
