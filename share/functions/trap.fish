# This defines a compatibility shim for the `trap` command found in other shells like bash and zsh.
function trap -d 'Perform an action when the shell receives a signal'
    set -l options h/help l/list-signals p/print
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
            for sig in (string upper -- $argv | string replace -r '^SIG' '')
                if test -n "$sig"
                    functions -e __trap_handler_$sig
                end
            end

        case set
            set -l cmd $argv[1]
            set -e argv[1]

            for sig in (string upper -- $argv | string replace -r '^SIG' '')
                if test -n "$sig"
                    set -l sw --on-signal $sig
                    if string match -qi exit -- $sig
                        set sw --on-event fish_exit
                    end
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
                set names (functions -a | string split ',' | string match "__trap_handler_*" | string replace '__trap_handler_' '')
            end

            for sig in (string upper -- $names | string replace -r '^SIG' '')
                if test -n "$sig"
                    functions __trap_handler_$sig
                    echo ""
                else
                    return 1
                end
            end

        case list
            kill -l
    end
end
