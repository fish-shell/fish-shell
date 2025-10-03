set -l bind_optspecs \
    a/all \
    e/erase \
    M/mode= \
    m/sets-mode= \
    preset \
    s/silent \
    user

function __fish_bind_has_keys --inherit-variable bind_optspecs
    argparse $bind_optspecs -- $argv 2>/dev/null
    or return
    test (count $argv) -ge 2
end

complete -c bind -f
complete -c bind -s a -l all -d 'Show unavailable key bindings/erase all bindings'
complete -c bind -s e -l erase -d 'Erase mode'
complete -c bind -s f -l function-names -d 'Print names of available functions'
complete -c bind -s h -l help -d "Display help and exit"
complete -c bind -s K -l key-names -d 'Print names of available keys'
complete -c bind -s M -l mode -d 'Specify the bind mode that the bind is used in' -xa '(bind -L)'
complete -c bind -s m -l sets-mode -d 'Change current mode after bind is executed' -xa '(bind -L)'
complete -c bind -s L -l list-modes -d 'Display a list of defined bind modes'
complete -c bind -s s -l silent -d 'Operate silently'
complete -c bind -l preset -d 'Operate on preset bindings'
complete -c bind -l user -d 'Operate on user bindings'

complete -c bind -n '__fish_bind_has_keys (commandline -pcx)' -a '(bind --function-names)' -d 'Function name' -x

function __fish_bind_complete --inherit-variable bind_optspecs
    argparse $bind_optspecs -- (commandline -xpc)[2..] 2>/dev/null
    or return 1
    set -l token (commandline -ct)
    if test (count $argv) = 0 && set -l prefix (string match -r -- '(.*,)?(ctrl-|alt-|shift-|super-)*' $token)
        printf '%salt-\tAlt modifier…\n' $prefix
        printf '%sctrl-\tCtrl modifier…\n' $prefix
        printf '%sshift-\tShift modifier…\n' $prefix
        printf '%ssuper-\tSuper modifier…\n' $prefix
        printf '%s\tNamed key\n' $prefix(bind --key-names)
    end
end
complete -c bind -k -a '(__fish_bind_complete)' -f
