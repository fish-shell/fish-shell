set -l COMMANDS -s i -o il -s u -o ul -o us -s l

complete -c gacutil -s '?' -d 'Show help'

complete -c gacutil -s i -c "! __fish_seen_argument $COMMANDS" \
    -d 'Install an assembly into the global assembly cache'
complete -c gacutil -o il -c "! __fish_seen_argument $COMMANDS" \
    -d 'Install one or more assemblies into the global assembly cache'
complete -c gacutil -s u -c "! __fish_seen_argument $COMMANDS" \
    -d 'Uninstall an assembly from the global assembly cache'
complete -c gacutil -o ul -c "! __fish_seen_argument $COMMANDS" \
    -d 'Uninstall one or more assemblies from the global assembly cache'
complete -c gacutil -o us -c "! __fish_seen_argument $COMMANDS" \
    -d 'Uninstall an assembly using the specifed assembly\'s full name'
complete -c gacutil -s l -c "! __fish_seen_argument $COMMANDS" \
    -d 'List the contents of the global assembly cache'

complete -c gacutil -o package -c '__fish_seen_argument $COMMANDS' \
    -d 'Create a directory in prefix/lib/mono'
complete -c gacutil -o gacdir -c "__fish_seen_argument $COMMANDS" \
    -d 'Use the GACs base directory'
complete -c gacutil -o root -c "__fish_seen_argument $COMMANDS" \
    -d 'Integrate with packaging tools that require a prefix directory to be specified'
complete -c gacutil -o check_refs -c '__fish_seen_argument -s i -o il' \
    -d 'Check the assembly being installed does not reference any non strong named assemblies'
