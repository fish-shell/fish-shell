# "add" is implicit.
set __fish_abbr_not_add_cond '__fish_seen_subcommand_from --query --rename --erase --show --list --help'
set __fish_abbr_add_cond 'not __fish_seen_subcommand_from --query --rename --erase --show --list --help'

complete -c abbr -f
complete -c abbr -f -n $__fish_abbr_not_add_cond -s a -l add -d 'Add abbreviation'
complete -c abbr -f -n $__fish_abbr_not_add_cond -s q -l query -d 'Check if an abbreviation exists'
complete -c abbr -f -n $__fish_abbr_not_add_cond -l rename -d 'Rename an abbreviation' -xa '(abbr --list)'
complete -c abbr -f -n $__fish_abbr_not_add_cond -s e -l erase -d 'Erase abbreviation' -xa '(abbr --list)'
complete -c abbr -f -n $__fish_abbr_not_add_cond -s s -l show -d 'Print all abbreviations'
complete -c abbr -f -n $__fish_abbr_not_add_cond -s l -l list -d 'Print all abbreviation names'
complete -c abbr -f -n $__fish_abbr_not_add_cond -s h -l help -d Help


complete -c abbr -f -n $__fish_abbr_add_cond -s p -l position -a 'command anywhere' -d 'Expand only as a command, or anywhere' -x
complete -c abbr -f -n $__fish_abbr_add_cond -s f -l function -d 'Treat value as a fish function'
complete -c abbr -f -n $__fish_abbr_add_cond -s r -l regex -d 'Match a regular expression' -x
complete -c abbr -f -n $__fish_abbr_add_cond -l set-cursor -d 'Position the cursor at % in the output'
