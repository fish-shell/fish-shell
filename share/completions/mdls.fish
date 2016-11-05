# completion for mdls (macOS)

complete -c mdls -s n -o name -x -d 'Print only the matching metadata attribute value'
complete -c mdls -s r -o raw -f -d 'Print raw attribute data'
complete -c mdls -n '__fish_seen_subcommand_from -raw -r' -o nullMarker -x -d 'Sets a marker string to be used when a requested attribute is null'
complete -c mdls -s p -o plist -r -d 'Output attributes in XML format to file'
