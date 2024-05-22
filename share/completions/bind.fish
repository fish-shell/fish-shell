function __fish_bind_test1
    set -l args
    set -l use_keys no
    for i in (commandline -pxc)
        switch $i
            case -k --k --ke --key
                set use_keys yes

            case "-*"

            case "*"
                set -a args $i
        end
    end

    switch $use_keys
        case yes
            switch (count $args)
                case 1
                    return 0
            end
    end
    return 1
end

function __fish_bind_test2
    set -l args
    for i in (commandline -pxc)
        switch $i
            case "-*"

            case "*"
                set -a args $i
        end
    end

    switch (count $args)
        case 2
            return 0
    end

    return 1

end

complete -c bind -f
complete -c bind -s a -l all -d 'Show unavailable key bindings/erase all bindings'
complete -c bind -s e -l erase -d 'Erase mode'
complete -c bind -s f -l function-names -d 'Print names of available functions'
complete -c bind -s h -l help -d "Display help and exit"
complete -c bind -s k -l key -d 'Specify key name, not sequence'
complete -c bind -s K -l key-names -d 'Print names of available keys'
complete -c bind -s M -l mode -d 'Specify the bind mode that the bind is used in' -xa '(bind -L)'
complete -c bind -s m -l sets-mode -d 'Change current mode after bind is executed' -xa '(bind -L)'
complete -c bind -s L -l list-modes -d 'Display a list of defined bind modes'
complete -c bind -s s -l silent -d 'Operate silently'
complete -c bind -l preset -d 'Operate on preset bindings'
complete -c bind -l user -d 'Operate on user bindings'

complete -c bind -n __fish_bind_test2 -a '(bind --function-names)' -d 'Function name' -x

function __fish_bind_complete
    argparse M/mode= m/sets-mode= preset user s/silent \
        a/all function-names list-modes e/erase -- (commandline -xpc)[2..] 2>/dev/null
    or return 1
    set -l token (commandline -ct)
    if test (count $argv) = 0 && set -l prefix (string match -r -- '(.*,)?(ctrl-|alt-|shift-)*' $token)
        printf '%sctrl-\tCtrl modifier…\n' $prefix
        printf '%sc-\tCtrl modifier…\n' $prefix
        printf '%salt-\tAlt modifier…\n' $prefix
        printf '%sa-\tAlt modifier…\n' $prefix
        printf '%sshift-\tShift modifier…\n' $prefix
        set -l key_names minus comma backspace delete escape \
            enter up down left right pageup pagedown home end insert tab \
            space f(seq 12)
        printf '%s\tNamed key\n' $prefix$key_names
    end
end
complete -c bind -k -a '(__fish_bind_complete)' -f
