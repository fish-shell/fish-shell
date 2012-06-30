function funced --description 'Edit function definition'
	set -l editor $EDITOR
    set -l interactive
    set -l funcname
    while set -q argv[1]
        switch $argv[1]
			case '---long impossible to match line, because case respects -h option' -h --help
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

    if begin; set -q funcname[2]; or not test "$funcname[1]"; end
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

    if begin; test "$editor" = fish; or set -q interactive[1]; end
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

    if not type -f "$editor" >/dev/null
        set_color red
        printf (_ "%s: Editor %s is not found\n") funced $editor
        set_color normal
    end

    set -q TMPDIR; or set -l TMPDIR /tmp
    set -l tmpname (printf "$TMPDIR/fish_funced_%d_%d.fish" %self (random))
    while test -f $tmpname
        set tmpname (printf "$TMPDIR/fish_funced_%d_%d.fish" %self (random))
    end

    if functions -q -- $funcname
        functions -- $funcname > $tmpname
    else
        echo $init > $tmpname
    end
    if eval $editor $tmpname
        . $tmpname
    end
    set -l stat $status 
    rm $tmpname >/dev/null
    return $stat
end
