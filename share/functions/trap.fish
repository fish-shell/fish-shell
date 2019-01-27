# This defines a compatibility shim for the `trap` command found in other shells like bash and zsh.

function __trap_translate_signal
    set upper (echo $argv[1]|tr a-z A-Z)
    string replace -r '^SIG' '' -- $upper
end

function __trap_switch
    switch $argv[1]
        case EXIT exit
            echo --on-event fish_exit

        case '*'
            echo --on-signal $argv[1]
    end
end

function trap -d 'Perform an action when the shell receives a signal'
    set -l options 'h/help' 'l/list-signals' 'p/print'
    argparse -n trap $options -- $argv
    or return

    if set -q _flag_help
        __fish_print_help trap
        return 0
    end

    set -l mode
    set -l cmd
    set -l sig

    # Determine the mode based on either an explicit flag or the non-flag args.
    if set -q _flag_print
        set mode print
    else if set -q _flag_list_signals
        set mode list
    else
        switch (count $argv)
            case 0
                set mode print
            case 1
                set mode clear
            case '*'
                if test $argv[1] = -
                    set -e argv[1]
                    set mode clear
                else
                    set mode set
                end
        end
    end

    switch $mode
        case clear
            for i in $argv
                set sig (__trap_translate_signal $i)
                if test -n "$sig"
                    functions -e __trap_handler_$sig
                end
            end

        case set
            set -l cmd $argv[1]
            set -e argv[1]

            for i in $argv
                set -l sig (__trap_translate_signal $i)
                set sw (__trap_switch $sig)

                if test -n "$sig"
                    echo "function __trap_handler_$sig $sw; $cmd; end" | source
                else
                    return 1
                end
            end

        case print
            set -l names
            if set -q argv[1]
                set names $argv
            else
                set names (functions -na | string match "__trap_handler_*" | string replace '__trap_handler_' '')
            end

            for i in $names
                set sig (__trap_translate_signal $i)

                if test -n "$sig"
                    functions __trap_handler_$i
                else
                    return 1
                end
            end

        case list
            kill -l
    end
end
