
# Checks if we are using GNU tools
if mkdir --version > /dev/null ^ /dev/null
    complete -c mkdir -l version --description 'Output version'
    complete -c mkdir -s m -l mode --description 'Set file mode (as in chmod)' -x	
    complete -c mkdir -s p -l parents --description 'Make parent directories as needed'
    complete -c mkdir -s v -l verbose --description 'Print a message for each created directory'
    complete -c mkdir -l help --description 'Display help'

else
    complete -c mkdir -s m --description 'Set file mode (as in chmod)' -x
    complete -c mkdir -s p --description 'Make parent directories as needed'
    complete -c mkdir -s v --description 'Print a message for each created directory'
end

# Checks if SELinux is installed
if command -s sestatus > /dev/null ^ /dev/null
    complete -c mkdir -l context -s Z --description 'Set SELinux security context of each created directory to the default type'
end
