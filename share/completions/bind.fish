
complete -c bind -s a -l all -d 'Show unavailable key bindings/erase all bindings'
complete -c bind -s e -l erase -d 'Erase mode'
complete -c bind -s f -l function-names -d 'Print names of available functions'
complete -c bind -s h -l help -d "Display help and exit"
complete -c bind -s k -l key -d 'Specify key name, not sequence'
complete -c bind -s K -l key-names -d 'Print names of available keys'
complete -c bind -s m -l mode -d 'Add to named bind mode'
complete -c bind -s M -l new-mode -d 'Change current bind mode to named mode'

complete -c bind -n __fish_bind_test1 -a '(bind --key-names)' -d 'Key name' -x
complete -c bind -n __fish_bind_test2 -a '(bind --function-names)' -d 'Function name' -x


