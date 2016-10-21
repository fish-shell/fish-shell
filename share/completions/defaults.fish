# completion for defaults (macOS)

function __fish_defaults_domains
    defaults domains | string split ", "
end

complete -f -c defaults -o 'currentHost' -d 'Restricts preferences operations to the host the user is currently logged in on'
complete -f -c defaults -o 'host' -d 'Restricts preferences operations to hostname'

# read
complete -f -c defaults -n '__fish_use_subcommand' -a read -d 'Shows defaults for all or the given domain'
complete -f -c defaults -n '__fish_seen_subcommand_from read read-type write rename delete' -a '(__fish_defaults_domains)'
complete -f -c defaults -n '__fish_seen_subcommand_from read read-type write rename delete' -o 'app'

# write
complete -f -c defaults -n '__fish_use_subcommand' -a write -d 'Writes domain or or a key in the domain'
complete -f -c defaults -n '__fish_seen_subcommand_from write' -o 'string' -d 'String as the value for the given preference key'
complete -f -c defaults -n '__fish_seen_subcommand_from write' -o 'data' -d 'Abunch of raw data bytes as the value for the given prefer ence key'
complete -f -c defaults -n '__fish_seen_subcommand_from write' -o 'int' -d 'Integer as the value for the given preference key'
complete -f -c defaults -n '__fish_seen_subcommand_from write' -o 'float' -d 'Floating point number as the value for the given preference key'
complete -f -c defaults -n '__fish_seen_subcommand_from write' -o 'bool' -d 'Boolean as the value for the given preference key'
complete -f -c defaults -n '__fish_seen_subcommand_from write' -o 'date' -d 'Date as the value for the given preference key'
complete -f -c defaults -n '__fish_seen_subcommand_from write' -o 'array' -d 'Array as the value for the given preference key'
complete -f -c defaults -n '__fish_seen_subcommand_from write' -o 'array-add' -d 'Allows to add new elements to the end of an array'
complete -f -c defaults -n '__fish_seen_subcommand_from write' -o 'dict' -d 'Dictionary to the defaults database for a domain'
complete -f -c defaults -n '__fish_seen_subcommand_from write' -o 'dict-add' -d 'Allows to add new key/value pairs to a dictionary'

complete -f -c defaults -n '__fish_use_subcommand' -a read-type -d 'Shows the type for the given domain, key'
complete -f -c defaults -n '__fish_use_subcommand' -a rename -d 'Renames old_key to new_key'
complete -f -c defaults -n '__fish_use_subcommand' -a delete -d 'Deletes domain or a key in the domain'
complete -f -c defaults -n '__fish_use_subcommand' -a domains -d 'Prints the names of all domains in the users defaults system'
complete -f -c defaults -n '__fish_use_subcommand' -a find -d 'Searches for word in the domain names, keys, and values of the users defaults'
complete -f -c defaults -n '__fish_use_subcommand' -a help -d 'Prints a list of possible command formats'
