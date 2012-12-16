
complete -c bind -s a -l all --description 'Show unavailable key bindings/erase all bindings'
complete -c bind -s e -l erase --description 'Erase mode'
complete -c bind -s f -l function-names --description 'Print names of available functions'
complete -c bind -s h -l help --description "Display help and exit"
complete -c bind -s k -l key --description 'Specify key name, not sequence'
complete -c bind -s K -l key-names --description 'Print names of available keys'

complete -c bind -n __fish_bind_test1 -a '(bind --key-names)' -d 'Key name' -x
complete -c bind -n __fish_bind_test2 -a '(bind --function-names)' -d 'Function name' -x


