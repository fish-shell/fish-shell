complete -c micro -o clean -d 'Cleans the configuration directory'
complete -c micro -o config-dir -xa '(__fish_complete_directories)' -d 'Custom location for the configuration directory'
complete -c micro -o options -d 'Show all option help'
complete -c micro -o debug -d 'Enable debug mode'
complete -c micro -o version -d 'Show version information'
complete -c micro -o help -d 'Show help message'

for option in (micro -options | string replace -f -r '^-(\S+)\s.*' '$1')
    complete -c micro -o $option -x
end

complete -c micro -o plugin -xa "
    install\t'Install plugins'
    remove\t'Remove plugins'
    update\t'Update plugins'
    search\t'Search for a plugin'
    list\t'List installed plugins'
    available\t'List available plugins'
"
