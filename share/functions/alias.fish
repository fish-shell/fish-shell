function alias --description 'Creates a function wrapping a command'
    set -l options 'h/help' 's/save'
    argparse -n alias --max-args=2 $options -- $argv
    or return

    if set -q _flag_help
        __fish_print_help alias
        return 0
    end

    set -l name
    set -l body
    set -l prefix
    set -l first_word
    set -l wrapped_cmd

    if not set -q argv[1]
        # Print the known aliases.
        for func in (functions -n)
            set -l output (functions $func | string match -r -- "^function .* --description 'alias (.*)'")
            if set -q output[2]
                set output (string replace -r -- '^'$func'[= ]' '' $output[2])
                echo alias $func (string escape -- $output[1])
            end
        end
        return 0
    else if not set -q argv[2]
        # Alias definition of the form "name=value".
        set -l tmp (string split -m 1 "=" -- $argv) ""
        set name $tmp[1]
        set body $tmp[2]
    else
        # Alias definition of the form "name value".
        set name $argv[1]
        set body $argv[2]
    end

    # sanity check
    if test -z "$name"
        printf ( _ "%s: Name cannot be empty\n") alias
        return 1
    else if test -z "$body"
        printf ( _ "%s: Body cannot be empty\n") alias
        return 1
    end

    # Extract the first command from the body.
    printf '%s\n' $body | read -lt first_word body

    # Prevent the alias from immediately running into an infinite recursion if
    # $body starts with the same command as $name.
    if test $first_word = $name
        if contains $name (builtin --names)
            set prefix builtin
        else
            set prefix command
        end
    end
    set -l cmd_string (string escape -- "alias $argv")
    set wrapped_cmd (string join ' ' -- $first_word $body | string escape)
    echo "function $name --wraps $wrapped_cmd --description $cmd_string; $prefix $first_word $body \$argv; end" | source
    if set -q _flag_save
        funcsave $name
    end
    #echo "function $name --wraps $wrapped_cmd --description $cmd_string; $prefix $first_word $body \$argv; end"
end
