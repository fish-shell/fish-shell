function __fish_print_xdg_desktop_file_ids --description 'Print all available xdg desktop file IDs'
    find (__fish_print_xdg_applications_directories) -name \*.desktop \( -type f -or -type l \) -printf '%P\n' | string replace -a -- / - | path sort -u
end

# main completion
complete -c xdg-mime -n 'not __fish_seen_subcommand_from query default install uninstall' -xa 'query default install uninstall'

# complete xdg-mime query
complete -c xdg-mime -n '__fish_seen_subcommand_from query; and not __fish_seen_subcommand_from filetype default' -xa 'filetype default' -d 'Query information'

# complete xdg-mime query default
complete -c xdg-mime -d 'Query default application for type' -n 'contains_seq query default -- (commandline -cxp)' -xa '(__fish_print_xdg_mimetypes)'

# complete xdg-mime query filetype
complete -c xdg-mime -d 'Query file\'s filetype' -n 'contains_seq query filetype -- (commandline -cxp)' -r

# complete xdg-mime default
complete -c xdg-mime -d 'Choose application' -n '__fish_seen_subcommand_from default; and test (commandline -xpc|count) -eq 2' -xa '(__fish_print_xdg_desktop_file_ids)'
complete -c xdg-mime -d Mimetype -n '__fish_seen_subcommand_from default; and test (commandline -xpc|count) -ge 3' -xa '(__fish_print_xdg_mimetypes)'

# complete xdg-mime install
complete -c xdg-mime -d 'Add filetype description' -n 'contains_seq xdg-mime install -- (commandline -cxp)' -r
complete -c xdg-mime -d 'Set mode' -n 'contains_seq xdg-mime install -- (commandline -cxp)' -l mode -xa 'user system'
complete -c xdg-mime -d 'Disable vendor check' -n 'contains_seq xdg-mime install -- (commandline -cxp)' -l novendor

# complete xdg-mime uninstall
complete -c xdg-mime -d 'Remove filetype description' -n 'contains_seq xdg-mime uninstall -- (commandline -cxp)' -r
complete -c xdg-mime -d 'Set mode' -n 'contains_seq xdg-mime uninstall -- (commandline -cxp)' -l mode -xa 'user system'

#complete -c xdg-mime install [--mode mode] [--novendor] mimetypes-file
#complete -c xdg-mime uninstall [--mode mode] mimetypes-file
complete -c xdg-mime -l help -d 'Display help'
complete -c xdg-mime -l manual -d 'Diplay long help'
complete -c xdg-mime -l version -d 'Print version'
