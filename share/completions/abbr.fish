complete -c abbr -f -s a -l add -d 'Add abbreviation'
# Abbr keys can't contain spaces, so we can safely replace the first space with a tab
# `abbr -s` won't work here because that already escapes
complete -c abbr -s e -l erase -d 'Erase abbreviation' -xa '(string replace " " \t -- $fish_user_abbreviations)'
complete -c abbr -f -s s -l show -d 'Print all abbreviations'
complete -c abbr -f -s l -l list -d 'Print all abbreviation names'
complete -c abbr -f -s h -l help -d 'Help'
