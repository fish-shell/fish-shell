complete -c mkdir -s m -l mode --description 'Set file mode (as in chmod)' -x
complete -c mkdir -s p -l parents --description 'Make parent directories as needed'
complete -c mkdir -s v -l verbose --description 'Print a message for each created directory'
complete -c mkdir -s Z --description 'Set SELinux security context of each created directory to the default type'
complete -c mkdir -l context --description 'Like -Z' -f
complete -c mkdir -l help --description 'Display help'
complete -c mkdir -l version --description 'Output version'


