function funced --description 'Edit function definition'
    set -l editor
    # Check VISUAL first since theoretically EDITOR could be ed
    if set -q VISUAL
        set editor $VISUAL
    else if set -q EDITOR
        set editor $EDITOR
    end
    set -l interactive
    set -l funcname
    while set -q argv[1]
        switch $argv[1]
            case -h --help
                __fish_print_help funced
                return 0

            case -e --editor
                set editor $argv[2]
                set -e argv[2]

            case -i --interactive
                set interactive 1

            case --
                set funcname $funcname $argv[2]
                set -e argv[2]

            case '-*'
                set_color red
                printf (_ "%s: Unknown option %s\n") funced $argv[1]
                set_color normal
                return 1

            case '*' '.*'
                set funcname $funcname $argv[1]
        end
        set -e argv[1]
    end

    if test (count $funcname) -ne 1
        set_color red
        _ "funced: You must specify one function name
"
        set_color normal
        return 1
    end

    set -l init
    switch $funcname
        case '-*'
        set init function -- $funcname\n\nend
        case '*'
        set init function $funcname\n\nend
    end

    # Break editor up to get its first command (i.e. discard flags)
    if test -n "$editor"
        set -l editor_cmd
        eval set editor_cmd $editor
        if not type -q -f "$editor_cmd[1]"
            _ "funced: The value for \$EDITOR '$editor' could not be used because the command '$editor_cmd[1]' could not be found
    "
            set editor fish
        end
    end
    
    # If no editor is specified, use fish
    if test -z "$editor"
        set editor fish
    end

    if begin; set -q interactive[1]; or test "$editor" = fish; end
        set -l IFS
        if functions -q -- $funcname
            # Shadow IFS here to avoid array splitting in command substitution
            set init (functions -- $funcname | fish_indent --no-indent)
        end

        set -l prompt 'printf "%s%s%s> " (set_color green) '$funcname' (set_color normal)'
        # Unshadow IFS since the fish_title breaks otherwise
        set -e IFS
        if read -p $prompt -c "$init" -s cmd
            # Shadow IFS _again_ to avoid array splitting in command substitution
            set -l IFS
            eval (echo -n $cmd | fish_indent)
        end
        return 0
    end

    # OSX mktemp is rather restricted - no suffix, no way to automatically use TMPDIR
    # Create a directory so we can use a ".fish" suffix for the file - makes editors pick up that it's a fish file
    set -q TMPDIR; or set -l TMPDIR /tmp
    set -l tmpdir (mktemp -d $TMPDIR/fish.XXXXXX)
    set -l tmpname $tmpdir/$funcname.fish

    if functions -q -- $funcname
        functions -- $funcname > $tmpname
    else
        echo $init > $tmpname
    end
        # Repeatedly edit until it either parses successfully, or the user cancels
        # If the editor command itself fails, we assume the user cancelled or the file
        # could not be edited, and we do not try again
        while true
            if not eval $editor $tmpname
                        _ "Editing failed or was cancelled"
                        echo
                else
                if not source $tmpname
                                # Failed to source the function file. Prompt to try again.
                                echo # add a line between the parse error and the prompt
                                set -l repeat
                                set -l prompt (_ 'Edit the file again\? [Y/n]')
                                while test -z "$repeat"
                                        read -p "echo $prompt\  " repeat
                                end
                                if not contains $repeat n N no NO No nO
                                        continue
                                end
                                _ "Cancelled function editing"
                                echo
                        end
                end
                break
        end
    set -l stat $status
    rm $tmpname >/dev/null
    and rmdir $tmpdir >/dev/null
    return $stat
end
