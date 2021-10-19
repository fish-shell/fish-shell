complete -c abbr -f
complete -c abbr -f -s a -l add -d 'Add abbreviation'
complete -c abbr -f -s q -l query -d 'Check if an abbreviation exists'
complete -c abbr -f -s r -l rename -d 'Rename an abbreviation' -xa '(abbr --list)'
complete -c abbr -f -s e -l erase -d 'Erase abbreviation' -xa '(abbr --list)'
complete -c abbr -f -s s -l show -d 'Print all abbreviations'
complete -c abbr -f -s l -l list -d 'Print all abbreviation names'
complete -c abbr -f -s g -l global -d 'Store abbreviation in a global variable'
complete -c abbr -f -s U -l universal -d 'Store abbreviation in a universal variable'
complete -c abbr -f -s h -l help -d Help
