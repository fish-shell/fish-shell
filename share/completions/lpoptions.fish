__fish_complete_lpr lpoptions

complete -c lpoptions -s d -d 'Sets the user default printer' -xa '(__fish_print_lpr_printers)'
complete -c lpoptions -s l -d 'Lists the printer specific options and their current settings'
complete -c lpoptions -s o -d 'Specifies a new option for the named destination' -xa '(__fish_complete_lpr_option)'
complete -c lpoptions -s p -d 'Sets the destination and instance for any options that follow' -xa '(__fish_print_lpr_printers)'
complete -c lpoptions -s r -d 'Removes the specified option for the named destination' -xa '(__fish_print_lpr_options)'
complete -c lpoptions -s x -d 'Removes the options for the named destination and instance' -xa '(__fish_print_lpr_printers)'
