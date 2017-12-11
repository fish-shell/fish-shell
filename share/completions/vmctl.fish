
complete -c vmctl -xa 'console create load log reload reset start status stop pause unpause send receive' -n 'not __fish_seen_subcommand_from list console create load log reload reset start status stop pause unpause send receive'
complete -c vmctl -n '__fish_seen_subcommand_from console reload reset start status stop pause unpause send receive' -xa '(vmctl status | string match -e -v "MAXMEM" | string replace -r "^(\s+\S+\s+){7}" "")'

