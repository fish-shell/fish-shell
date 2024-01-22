# Fish completions for glibs's "gsettings" configuration tool

function __fish_complete_gsettings_args
    set -l schema_commands get monitor writable range describe set reset reset-recursively list-keys list-children list-recursively
    set -l key_commands get monitor writable range describe set reset

    set -l schemadir
    set -l cmd (commandline -pxc)
    set -e cmd[1]

    if set -q cmd[2]
        and string match -q -- --schemadir $cmd[1]
        # TODO: This needs to support proper expansion of paths (~, variables, etc.)
        set schemadir --schemadir $cmd[2]
        set -e cmd[1..2]
    end

    if not set -q cmd[1]
        return 1
    end

    set -l subcommand $cmd[1]

    if not contains -- $subcommand $schema_commands
        return 1
    end

    if not set -q cmd[2]
        # Non-relocatable schemas
        gsettings $schemadir list-schemas 2>/dev/null

        # Relocatable schemas require a dconf path, but there is no universal way of
        # finding the right paths to suggest here. Just default to '/'.
        gsettings $schemadir list-relocatable-schemas 2>/dev/null | string replace -r \$ ':/'
        return 0
    end

    if not contains -- $subcommand $key_commands
        return 1
    end

    set -l schema $cmd[2]

    if not set -q cmd[3]
        gsettings $schemadir list-keys $schema 2>/dev/null
        return 0
    end

    if test $subcommand != set
        return 1
    end

    if not set -q cmd[4]
        set -l key $cmd[3]
        set -l range (gsettings $schemadir range $schema $key 2>/dev/null)
        set -l key_type $range[1]
        set -e range[1]
        switch $key_type
            case 'type b'
                echo true
                echo false
            case enum
                string join \n $range
            case '*'
                # If no sensible suggestions can be made, just use the current value.
                # It gives a good indication on the expected format and is likely already close to what the user wants.
                gsettings $schemadir get $schema $key 2>/dev/null
        end
        return 0
    end

    return 1
end

set -l valid_commands get monitor writable range describe set reset reset-recursively list-schemas list-relocatable-schemas list-keys list-children list-recursively help

complete -f -c gsettings

complete -f -c gsettings -n "not __fish_seen_subcommand_from $valid_commands" -l version -d 'Print the version information'
complete -c gsettings -n "not __fish_seen_subcommand_from $valid_commands" -l schemadir -d 'Specify a custom schemas directory' -xa "(__fish_complete_directories (commandline -ct))"

complete -f -c gsettings -n "not __fish_seen_subcommand_from $valid_commands" -a get -d 'Get the value of a key'
complete -f -c gsettings -n "not __fish_seen_subcommand_from $valid_commands" -a writable -d 'Determine if a key is writable'
complete -f -c gsettings -n "not __fish_seen_subcommand_from $valid_commands" -a range -d 'Determine the valid value range of a key'
complete -f -c gsettings -n "not __fish_seen_subcommand_from $valid_commands" -a describe -d 'Print the description of a key'
complete -f -c gsettings -n "not __fish_seen_subcommand_from $valid_commands" -a set -d 'Set the value of a key'
complete -f -c gsettings -n "not __fish_seen_subcommand_from $valid_commands" -a reset -d 'Reset a key to its default value'
complete -f -c gsettings -n "not __fish_seen_subcommand_from $valid_commands" -a reset-recursively -d 'Reset all keys under the given schema'
complete -f -c gsettings -n "not __fish_seen_subcommand_from $valid_commands" -a list-schemas -d 'List all installed, non-relocatable schemas'
complete -f -c gsettings -n "not __fish_seen_subcommand_from $valid_commands" -a list-relocatable-schemas -d 'List all installed, relocatable schemas'
complete -f -c gsettings -n "not __fish_seen_subcommand_from $valid_commands" -a list-keys -d 'List all keys in a schema'
complete -f -c gsettings -n "not __fish_seen_subcommand_from $valid_commands" -a list-children -d 'List all children of a schema'
complete -f -c gsettings -n "not __fish_seen_subcommand_from $valid_commands" -a list-recursively -d 'List keys and values, recursively'
complete -f -c gsettings -n "not __fish_seen_subcommand_from $valid_commands" -a help -d 'Print help'

complete -f -c gsettings -n "__fish_seen_subcommand_from $valid_commands" -xa "(__fish_complete_gsettings_args)"
