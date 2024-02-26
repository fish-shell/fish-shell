# Checks if we are using GNU tools
if mkdir --version >/dev/null 2>/dev/null
    complete -c mkdir -l version -d 'Output version'
    complete -c mkdir -s m -l mode -d 'Set file mode (as in chmod)' -x
    complete -c mkdir -s p -l parents -d 'Make parent directories as needed'
    complete -c mkdir -s v -l verbose -d 'Print a message for each created directory'
    complete -c mkdir -l help -d 'Display help'

else
    complete -c mkdir -s m -d 'Set file mode (as in chmod)' -x
    complete -c mkdir -s p -d 'Make parent directories as needed'
    complete -c mkdir -s v -d 'Print a message for each created directory'
end

# Checks if SELinux is installed
if command -s sestatus >/dev/null 2>/dev/null
    complete -c mkdir -l context -s Z -d 'Set SELinux security context of each created directory to the default type'
end
