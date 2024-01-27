function __fish_qdbus_complete
    argparse system 'bus=' literal help -- (commandline --cut-at-cursor --tokens-expanded) 2>/dev/null
    or return
    if set -q _flag_help
        return
    end
    set -l qdbus_flags $_flag_system
    if set -q _flag_bus
        set -a qdbus_flags --bus $_flag_bus
    end
    set argc (count $argv)
    if test $argc -le 3
        # avoid completion of property value
        qdbus $qdbus_flags $argv[2] $argv[3] | string replace --regex '^(?<kind>property\ (read)?(write)?|signal|method( Q_NOREPLY)?) (?<type>(\{.+\})|([^\ ]+)) (?<name>[^\(]+)(?<arguments>\(.+?\))?' '$name\t$kind $type $arguments' | string trim
    end
end

complete -c qdbus -f

complete -c qdbus -l system -d 'connect to the system bus'
complete -c qdbus -l bus -r -d 'connect to a custom bus'
complete -c qdbus -l literal -d 'print replies literally'
complete -c qdbus -l help -d 'print usage'

complete -c qdbus -a '(__fish_qdbus_complete)'
